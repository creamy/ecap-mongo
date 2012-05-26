/* ********************************************************************************

adapter_mongo.cc

Waitman Gobble
San Jose, California USA
waitman@waitman.net

NO WARRANTIES

based on adapter_passthru.cc from www.e-cap.org samples, 
      The Measurement Factory

******************************************************************************** */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>

#include <libecap/common/message.h>
#include <libecap/common/registry.h>
#include <libecap/common/errors.h>
#include <libecap/common/header.h>
#include <libecap/common/names.h>
#include <libecap/adapter/service.h>
#include <libecap/adapter/xaction.h>
#include <libecap/host/xaction.h>
#include <boost/uuid/uuid.hpp>            
#include <boost/uuid/uuid_generators.hpp> 
#include <boost/uuid/uuid_io.hpp>         


#include "client/dbclient.h"

namespace Adapter { // not required, but adds clarity

using libecap::size_type;

class Service: public libecap::adapter::Service {
	public:
		// About
		virtual std::string uri() const; // unique across all vendors
		virtual std::string tag() const; // changes with version and config
		virtual void describe(std::ostream &os) const; // free-format info

		// Configuration
		virtual void configure(const libecap::Options &cfg);
		virtual void reconfigure(const libecap::Options &cfg);

		// Lifecycle
		virtual void start(); // expect makeXaction() calls
		virtual void stop(); // no more makeXaction() calls until start()
		virtual void retire(); // no more makeXaction() calls

		// Scope (XXX: this may be changed to look at the whole header)
		virtual bool wantsUrl(const char *url) const;

		// Work
		virtual libecap::adapter::Xaction *makeXaction(libecap::host::Xaction *hostx);

};


class Xaction: public libecap::adapter::Xaction {
	public:
		Xaction(libecap::host::Xaction *x);
		virtual ~Xaction();

		// meta-information for the host transaction
		virtual const libecap::Area option(const libecap::Name &name) const;
		virtual void visitEachOption(libecap::NamedValueVisitor &visitor) const;

		// lifecycle
		virtual void start();
		virtual void stop();

		// adapted body transmission control
		virtual void abDiscard();
		virtual void abMake();
		virtual void abMakeMore();
		virtual void abStopMaking();

		// adapted body content extraction and consumption
		virtual libecap::Area abContent(size_type offset, size_type size);
		virtual void abContentShift(size_type size);

		// virgin body state notification
		virtual void noteVbContentDone(bool atEnd);
		virtual void noteVbContentAvailable();

		// libecap::Callable API, via libecap::host::Xaction
		virtual bool callable() const;

	protected:
		libecap::host::Xaction *lastHostCall(); // clears hostx

	private:
		libecap::host::Xaction *hostx; // Host transaction rep

		std::string	dzBuffer;
		std::string	dzContentType;
		std::string	dzContentID;
		int			chunkID;
				
		typedef enum { opUndecided, opOn, opComplete, opNever } OperationState;
		OperationState receivingVb;
		OperationState sendingAb;
};

} // namespace Adapter


std::string Adapter::Service::uri() const {
	return "ecap://cudapang.com/ecap/services/mongo";
}

std::string Adapter::Service::tag() const {
	return "0.0.0";
}

void Adapter::Service::describe(std::ostream &os) const {
	os << "A mongodb adapter for squid v0.0.0";
}

void Adapter::Service::configure(const libecap::Options &) {
	// this service is not configurable
}

void Adapter::Service::reconfigure(const libecap::Options &) {
	// this service is not configurable
}

void Adapter::Service::start() {
	libecap::adapter::Service::start();
	// custom code would go here, but this service does not have one
}

void Adapter::Service::stop() {
	// custom code would go here, but this service does not have one
	libecap::adapter::Service::stop();
}

void Adapter::Service::retire() {
	// custom code would go here, but this service does not have one
	libecap::adapter::Service::stop();
}

bool Adapter::Service::wantsUrl(const char *url) const {
	return true;
}

libecap::adapter::Xaction *Adapter::Service::makeXaction(libecap::host::Xaction *hostx) {
	return new Adapter::Xaction(hostx);
}


Adapter::Xaction::Xaction(libecap::host::Xaction *x): hostx(x),
	receivingVb(opUndecided), sendingAb(opUndecided) {
}

Adapter::Xaction::~Xaction() {
	if (libecap::host::Xaction *x = hostx) {
		hostx = 0;
		x->adaptationAborted();
	}

}

const libecap::Area Adapter::Xaction::option(const libecap::Name &) const {
	return libecap::Area(); // this transaction has no meta-information
}

void Adapter::Xaction::visitEachOption(libecap::NamedValueVisitor &) const {
	// this transaction has no meta-information to pass to the visitor
}

void Adapter::Xaction::start() {
	
	// TODO: libecap should probably supply a global LastCall() of sorts
	// to clear hostx member and then call the host transaction one last time
	Must(hostx);
	if (hostx->virgin().body()) {
		receivingVb = opOn;
		chunkID=0;
		dzBuffer="";
		hostx->vbMake(); // ask host to supply virgin body
	} else {
		receivingVb = opNever;
	}

	libecap::shared_ptr<libecap::Message> adapted = hostx->virgin().clone();
	Must(adapted != 0);

	static const libecap::Name contentTypeName("Content-Type");
	
	if(adapted->header().hasAny(contentTypeName)) {
		const libecap::Header::Value contentType = adapted->header().value(contentTypeName);
		
		if(contentType.size > 0) {
			dzContentType = contentType.toString(); // expensive
		}
	}
	
	if (!adapted->body()) {
		sendingAb = opNever; // there is nothing to send
		lastHostCall()->useAdapted(adapted);
	} else {
		hostx->useAdapted(adapted);
	} 
	
}

void Adapter::Xaction::stop() {
	hostx = 0;
	// the caller will delete
}

void Adapter::Xaction::abDiscard()
{
	Must(sendingAb == opUndecided); // have not started yet
	sendingAb = opNever;
}

void Adapter::Xaction::abMake()
{
	Must(sendingAb == opUndecided); // have not yet started or decided not to send
	Must(hostx->virgin().body()); // that is our only source of ab content

	// we are or were receiving vb
	Must(receivingVb == opOn || receivingVb == opComplete);
	
	sendingAb = opOn;
	if (!dzBuffer.empty())
		hostx->noteAbContentAvailable();
}

void Adapter::Xaction::abMakeMore()
{
	Must(receivingVb == opOn); // a precondition for receiving more vb
	hostx->vbMakeMore();
}

void Adapter::Xaction::abStopMaking()
{
	sendingAb = opComplete;
	// we may still continue receiving
}


libecap::Area Adapter::Xaction::abContent(size_type offset, size_type size) {

	// required to not raise an exception on the final call with opComplete
	Must(sendingAb == opOn || sendingAb == opComplete);

	// if complete, there is nothing more to return.
	if (sendingAb == opComplete) {
		return libecap::Area::FromTempString("");
	}
	return libecap::Area::FromTempString(dzBuffer.substr(offset, size));
}

void Adapter::Xaction::abContentShift(size_type size) {
	Must(sendingAb == opOn || sendingAb == opComplete);
	dzBuffer.erase(0, size);
}


void Adapter::Xaction::noteVbContentDone(bool atEnd) {	
		
	Must(receivingVb == opOn);
	receivingVb = opComplete;
	
	if (sendingAb == opOn) {
		hostx->noteAbContentDone(atEnd);
		sendingAb = opComplete;
	}
	
}

void Adapter::Xaction::noteVbContentAvailable() {
	
	Must(receivingVb == opOn);

	const libecap::Area vb = hostx->vbContent(0, libecap::nsize); // get all vb
	std::string chunk = vb.toString(); // expensive, but simple
	hostx->vbContentShift(vb.size); 
	dzBuffer += chunk; 
	
	//store in db
	//generate contentID if it does not exist
	
	mongo::DBClientConnection c;
	c.connect("127.0.0.1");
	mongo::BSONObjBuilder b;
	if (dzContentID=="") {
		std::stringstream suuid;
		boost::uuids::uuid uuid = boost::uuids::random_generator()();
		suuid << uuid;
		dzContentID = suuid.str(); 
	} 
	b.append( "cid", dzContentID );
	b.append( "chid", chunkID++ );
	b.append( "ct", dzContentType );
	b.append( "cs", chunk );
//	b.append( "ip", libecap::host::Xaction::option(libecap::metaClientIp) );
	c.insert("monitor.requests", b.obj() );
	
	if (sendingAb == opOn)
		hostx->noteAbContentAvailable();
}


bool Adapter::Xaction::callable() const {
	return hostx != 0; // no point to call us if we are done
}

// this method is used to make the last call to hostx transaction
// last call may delete adapter transaction if the host no longer needs it
// TODO: replace with hostx-independent "done" method
libecap::host::Xaction *Adapter::Xaction::lastHostCall() {
	libecap::host::Xaction *x = hostx;
	Must(x);
	hostx = 0;
	return x;
}

// create the adapter and register with libecap to reach the host application
static const bool Registered = (libecap::RegisterService(new Adapter::Service), true);



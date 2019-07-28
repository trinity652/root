/// \file RWebWindowWSHandler.hxx
/// \ingroup WebGui ROOT7
/// \author Sergey Linev <s.linev@gsi.de>
/// \date 2018-08-20
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!

/*************************************************************************
 * Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT7_RWebWindowWSHandler
#define ROOT7_RWebWindowWSHandler

#include "THttpWSHandler.h"

#include <ROOT/RWebWindow.hxx>

#include <string>

using namespace std::string_literals;

namespace ROOT {
namespace Experimental {

/// just wrapper to deliver websockets call-backs to the RWebWindow class

class RWebWindowWSHandler : public THttpWSHandler {

protected:
   Bool_t ProcessBatchHolder(std::shared_ptr<THttpCallArg> &arg) override
   {
      return IsDisabled() ? kFALSE : fWindow.ProcessBatchHolder(arg);
   }

   void VerifyDefaultPageContent(std::shared_ptr<THttpCallArg> &arg) override
   {
      auto version = fWindow.GetClientVersion();
      if (!version.empty()) {
         std::string search = "jsrootsys/scripts/JSRootCore."s;
         std::string replace = version + "/jsrootsys/scripts/JSRootCore."s;
         // replace link to JSROOT main script to emulate new version
         arg->ReplaceAllinContent(search, replace, true);
         arg->AddNoCacheHeader();
      }
   }

public:
   RWebWindow &fWindow; ///<! window reference

   /// constructor
   RWebWindowWSHandler(RWebWindow &wind, const char *name)
      : THttpWSHandler(name, "RWebWindow websockets handler", kFALSE), fWindow(wind)
   {
   }

   virtual ~RWebWindowWSHandler() = default;

   /// returns content of default web-page
   /// THttpWSHandler interface
   TString GetDefaultPageContent() override { return IsDisabled() ? "" : fWindow.fDefaultPage.c_str(); }

   /// returns true when window allowed to serve files relative to default page
   Bool_t CanServeFiles() const override { return !IsDisabled(); }

   /// Process websocket request - called from THttpServer thread
   /// THttpWSHandler interface
   Bool_t ProcessWS(THttpCallArg *arg) override { return arg && !IsDisabled() ? fWindow.ProcessWS(*arg) : kFALSE; }

   /// Allow processing of WS actions in arbitrary thread
   Bool_t AllowMTProcess() const override { return fWindow.fProcessMT; }

   /// Allows usage of special threads for send operations
   Bool_t AllowMTSend() const override { return fWindow.fSendMT; }

   /// React on completion of multithreaded send operation
   void CompleteWSSend(UInt_t wsid) override { if (!IsDisabled()) fWindow.CompleteWSSend(wsid); }
};

} // namespace Experimental
} // namespace ROOT

#endif

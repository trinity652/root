/// \file RWebDisplayArgs.cxx
/// \ingroup WebGui ROOT7
/// \author Sergey Linev <s.linev@gsi.de>
/// \date 2018-10-24
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!

/*************************************************************************
 * Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <ROOT/RWebDisplayArgs.hxx>

#include "TROOT.h"

/** \class ROOT::Experimental::RWebDisplayArgs
 * \ingroup webdisplay
 *
 * Holds different arguments for starting browser with RWebDisplayHandle::Display() method
 */

///////////////////////////////////////////////////////////////////////////////////////////
/// Default constructor - browser kind configured from gROOT->GetWebDisplay()

ROOT::Experimental::RWebDisplayArgs::RWebDisplayArgs()
{
   SetBrowserKind("");
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Constructor - browser kind specified as std::string
/// See SetBrowserKind() method for description of allowed parameters

ROOT::Experimental::RWebDisplayArgs::RWebDisplayArgs(const std::string &browser)
{
   SetBrowserKind(browser);
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Constructor - browser kind specified as const char *
/// See SetBrowserKind() method for description of allowed parameters

ROOT::Experimental::RWebDisplayArgs::RWebDisplayArgs(const char *browser)
{
   SetBrowserKind(browser);
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Set browser kind as string argument
/// Recognized values:
///  chrome  - use Google Chrome web browser, supports headless mode from v60, default
///  firefox - use Mozilla Firefox browser, supports headless mode from v57
///   native - (or empty string) either chrome or firefox, only these browsers support batch (headless) mode
///  browser - default system web-browser, no batch mode
///      cef - Chromium Embeded Framework, local display, local communication
///      qt5 - Qt5 WebEngine, local display, local communication
///    local - either cef or qt5
///   <prog> - any program name which will be started instead of default browser, like /usr/bin/opera

void ROOT::Experimental::RWebDisplayArgs::SetBrowserKind(const std::string &_kind)
{
   std::string kind = _kind;

   auto pos = kind.find("?");
   if (pos == 0) {
      SetUrlOpt(kind.substr(1));
      kind.clear();
   }

   if (kind.empty())
      kind = gROOT->GetWebDisplay().Data();

   if (kind == "local")
      SetBrowserKind(kLocal);
   else if (kind.empty() || (kind == "native"))
      SetBrowserKind(kNative);
   else if (kind == "firefox")
      SetBrowserKind(kFirefox);
   else if ((kind == "chrome") || (kind == "chromium"))
      SetBrowserKind(kChrome);
   else if ((kind == "cef") || (kind == "cef3"))
      SetBrowserKind(kCEF);
   else if ((kind == "qt") || (kind == "qt5"))
      SetBrowserKind(kQt5);
   else
      SetCustomExec(kind);
}

/////////////////////////////////////////////////////////////////////
/// Returns configured browser name

std::string ROOT::Experimental::RWebDisplayArgs::GetBrowserName() const
{
   switch (GetBrowserKind()) {
      case kChrome: return "chrome";
      case kFirefox: return "firefox";
      case kNative: return "native";
      case kCEF: return "cef";
      case kQt5: return "qt5";
      case kLocal: return "local";
      case kStandard: return "default";
      case kCustom:
          auto pos = fExec.find(" ");
          return (pos == std::string::npos) ? fExec : fExec.substr(0,pos);
   }

   return "";
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Append string to url options
/// Add "&" as separator if any options already exsists

void ROOT::Experimental::RWebDisplayArgs::AppendUrlOpt(const std::string &opt)
{
   if (opt.empty()) return;

   if (!fUrlOpt.empty())
      fUrlOpt.append("&");

   fUrlOpt.append(opt);
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Returns full url, which is combined from URL and extra URL options
/// Takes into account "#" symbol in url - options are inserted before that symbol

std::string ROOT::Experimental::RWebDisplayArgs::GetFullUrl() const
{
   std::string url = GetUrl(), urlopt = GetUrlOpt();
   if (url.empty() || urlopt.empty()) return url;

   auto rpos = url.find("#");
   if (rpos == std::string::npos) rpos = url.length();

   if (url.find("?") != std::string::npos)
      url.insert(rpos, "&");
   else
      url.insert(rpos, "?");
   url.insert(rpos+1, urlopt);

   return url;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// Configure custom web browser
/// Either just name of browser which can be used like "opera"
/// or full execution string which must includes $url like "/usr/bin/opera $url"

void ROOT::Experimental::RWebDisplayArgs::SetCustomExec(const std::string &exec)
{
   SetBrowserKind(kCustom);
   fExec = exec;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// returns custom executable to start web browser

std::string ROOT::Experimental::RWebDisplayArgs::GetCustomExec() const
{
   return GetBrowserKind() == kCustom ? fExec : "";
}

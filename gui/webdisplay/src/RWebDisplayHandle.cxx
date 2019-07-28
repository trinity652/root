/// \file RWebDisplayHandle.cxx
/// \ingroup WebGui ROOT7
/// \author Sergey Linev <s.linev@gsi.de>
/// \date 2018-10-17
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!

/*************************************************************************
 * Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <ROOT/RWebDisplayHandle.hxx>

#include <ROOT/RMakeUnique.hxx>
#include <ROOT/RLogger.hxx>

#include "RConfigure.h"
#include "TSystem.h"
#include "TRandom.h"
#include "TString.h"
#include "TObjArray.h"
#include "TEnv.h"

#include <regex>

#ifdef _MSC_VER
#include <process.h>
#else
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <spawn.h>
#endif

using namespace std::string_literals;

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Static holder of registered creators of web displays

std::map<std::string, std::unique_ptr<ROOT::Experimental::RWebDisplayHandle::Creator>> &ROOT::Experimental::RWebDisplayHandle::GetMap()
{
   static std::map<std::string, std::unique_ptr<ROOT::Experimental::RWebDisplayHandle::Creator>> sMap;
   return sMap;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Search for specific browser creator
/// If not found, try to add one
/// \param name - creator name like ChromeCreator
/// \param libname - shared library name where creator could be provided

std::unique_ptr<ROOT::Experimental::RWebDisplayHandle::Creator> &ROOT::Experimental::RWebDisplayHandle::FindCreator(const std::string &name, const std::string &libname)
{
   auto &m = GetMap();
   auto search = m.find(name);
   if (search == m.end()) {

      if (libname == "ChromeCreator") {
         m.emplace(name, std::make_unique<ChromeCreator>());
      } else if (libname == "FirefoxCreator") {
         m.emplace(name, std::make_unique<FirefoxCreator>());
      } else if (libname == "BrowserCreator") {
         m.emplace(name, std::make_unique<BrowserCreator>(false));
      } else if (!libname.empty()) {
         gSystem->Load(libname.c_str());
      }

      search = m.find(name); // try again
   }

   if (search != m.end())
      return search->second;

   static std::unique_ptr<ROOT::Experimental::RWebDisplayHandle::Creator> dummy;
   return dummy;
}

namespace ROOT {
namespace Experimental {

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Specialized handle to hold information about running browser process
/// Used to correctly cleanup all processes and temporary directories

class RWebBrowserHandle : public RWebDisplayHandle {

#ifdef _MSC_VER
   typedef int browser_process_id;
#else
   typedef pid_t browser_process_id;
#endif
   std::string fTmpDir;
   bool fHasPid{false};
   browser_process_id fPid;

public:
   RWebBrowserHandle(const std::string &url, const std::string &tmpdir) : RWebDisplayHandle(url), fTmpDir(tmpdir) {}

   RWebBrowserHandle(const std::string &url, const std::string &tmpdir, browser_process_id pid)
      : RWebDisplayHandle(url), fTmpDir(tmpdir), fHasPid(true), fPid(pid)
   {
   }

   virtual ~RWebBrowserHandle()
   {
#ifdef _MSC_VER
      if (fHasPid)
         gSystem->Exec(("taskkill /F /PID "s + std::to_string(fPid)).c_str());
      std::string rmdir = "rmdir /S /Q ";
#else
      if (fHasPid)
         kill(fPid, SIGKILL);
      std::string rmdir = "rm -rf ";
#endif
      if (!fTmpDir.empty())
         gSystem->Exec((rmdir + fTmpDir).c_str());
   }
};

} // namespace Experimental
} // namespace ROOT

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Class to handle starting of web-browsers like Chrome or Firefox

ROOT::Experimental::RWebDisplayHandle::BrowserCreator::BrowserCreator(bool custom, const std::string &exec)
{
   if (custom) return;

   if (!exec.empty()) {
      if (exec.find("$url") == std::string::npos) {
         fProg = exec;
#ifdef _MSC_VER
         fExec = exec + " $url";
#else
         fExec = exec + " $url &";
#endif
      } else {
         fExec = exec;
         auto pos = exec.find(" ");
         if (pos != std::string::npos)
            fProg = exec.substr(0, pos);
      }
   } else if (gSystem->InheritsFrom("TMacOSXSystem")) {
      fExec = "open \'$url\'";
   } else if (gSystem->InheritsFrom("TWinNTSystem")) {
      fExec = "start $url";
   } else {
      fExec = "xdg-open \'$url\' &";
   }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Check if browser executable exists and can be used

void ROOT::Experimental::RWebDisplayHandle::BrowserCreator::TestProg(const std::string &nexttry, bool check_std_paths)
{
   if (nexttry.empty() || !fProg.empty())
      return;

   if (!gSystem->AccessPathName(nexttry.c_str(), kExecutePermission)) {
#ifdef R__MACOSX
      fProg = std::regex_replace(nexttry, std::regex("%20"), " ");
#else
      fProg = nexttry;
#endif
      return;
   }

   if (!check_std_paths)
      return;

#ifdef _MSC_VER
   std::string ProgramFiles = gSystem->Getenv("ProgramFiles");
   auto pos = ProgramFiles.find(" (x86)");
   if (pos != std::string::npos)
      ProgramFiles.erase(pos, 6);
   std::string ProgramFilesx86 = gSystem->Getenv("ProgramFiles(x86)");

   if (!ProgramFiles.empty())
      TestProg(ProgramFiles + nexttry, false);
   if (!ProgramFilesx86.empty())
      TestProg(ProgramFilesx86 + nexttry, false);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Display given URL in web browser

std::unique_ptr<ROOT::Experimental::RWebDisplayHandle>
ROOT::Experimental::RWebDisplayHandle::BrowserCreator::Display(const RWebDisplayArgs &args)
{
   std::string url = args.GetFullUrl();
   if (url.empty())
      return nullptr;

   std::string exec;
   if (args.IsHeadless())
      exec = fBatchExec;
   else if (args.IsStandalone())
      exec = fExec;
   else
#ifdef _MSC_VER
      exec = "$prog $url";
#else
      exec = "$prog $url &";
#endif

   if (exec.empty())
      return nullptr;

   std::string swidth = std::to_string(args.GetWidth() > 0 ? args.GetWidth() : 800);
   std::string sheight = std::to_string(args.GetHeight() > 0 ? args.GetHeight() : 600);

   std::string rmdir = MakeProfile(exec, args.IsHeadless());

   exec = std::regex_replace(exec, std::regex("\\$url"), url);
   exec = std::regex_replace(exec, std::regex("\\$width"), swidth);
   exec = std::regex_replace(exec, std::regex("\\$height"), sheight);

   if (exec.compare(0,5,"fork:") == 0) {
      if (fProg.empty()) {
         R__ERROR_HERE("WebDisplay") << "Fork instruction without executable";
         return nullptr;
      }

      exec.erase(0, 5);

#ifndef _MSC_VER

      std::unique_ptr<TObjArray> fargs(TString(exec.c_str()).Tokenize(" "));
      if (!fargs || (fargs->GetLast()<=0)) {
         R__ERROR_HERE("WebDisplay") << "Fork instruction is empty";
         return nullptr;
      }

      std::vector<char *> argv;
      argv.push_back((char *) fProg.c_str());
      for (Int_t n = 0; n <= fargs->GetLast(); ++n)
         argv.push_back((char *)fargs->At(n)->GetName());
      argv.push_back(nullptr);

      R__DEBUG_HERE("WebDisplay") << "Show web window in browser with posix_spawn:\n" << fProg << " " << exec;

      pid_t pid;
      int status = posix_spawn(&pid, argv[0], nullptr, nullptr, argv.data(), nullptr);
      if (status != 0) {
         R__ERROR_HERE("WebDisplay") << "Fail to launch " << argv[0];
         return 0;
      }

      // add processid and rm dir

      return std::make_unique<RWebBrowserHandle>(url, rmdir, pid);

      // return win.AddProcId(batch_mode, key, std::string("pid:") + std::to_string((int)pid) + rmdir);

#else
      std::string tmp;
      char c;
      int pid;
      if (!fProg.empty()) {
         exec = "wmic process call create \""s + fProg + exec;
      } else {
         R__ERROR_HERE("WebDisplay") << "No Web browser found in Program Files!";
         return nullptr;
      }
      exec.append("\" | find \"ProcessId\" ");
      std::string process_id = gSystem->GetFromPipe(exec.c_str());
      std::stringstream ss(process_id);
      ss >> tmp >> c >> pid;

      // add processid and rm dir
      return std::make_unique<RWebBrowserHandle>(url, rmdir, pid);

      //return win.AddProcId(batch_mode, key, std::string("pid:") + std::to_string((int)pid) + rmdir);
#endif
   }

#ifdef _MSC_VER
   std::vector<char *> argv;
   std::string firstarg = fProg;
   while(firstarg.find("\\") != std::string::npos)
      firstarg.erase(0, firstarg.find("\\")+1);
   argv.push_back((char *)firstarg.c_str());

   std::unique_ptr<TObjArray> fargs(TString(exec.c_str()).Tokenize(" "));
   for (Int_t n = 1; n <= fargs->GetLast(); ++n)
      argv.push_back((char *)fargs->At(n)->GetName());
   argv.push_back(nullptr);

   R__DEBUG_HERE("WebDisplay") << "Showing web window in " << fProg << " with:\n" << exec;

   _spawnv(_P_NOWAIT, fProg.c_str(), argv.data());

#else

#ifdef R__MACOSX
   std::string prog = std::regex_replace(fProg, std::regex(" "), "\\ ");
#else
   std::string prog = fProg;
#endif

   exec = std::regex_replace(exec, std::regex("\\$prog"), prog);

   R__DEBUG_HERE("WebDisplay") << "Showing web window in browser with:\n" << exec;

   gSystem->Exec(exec.c_str());
#endif

   // add rmdir if required
   return std::make_unique<RWebBrowserHandle>(url, rmdir);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Constructor

ROOT::Experimental::RWebDisplayHandle::ChromeCreator::ChromeCreator() : BrowserCreator(true)
{
   TestProg(gEnv->GetValue("WebGui.Chrome", ""));

#ifdef _MSC_VER
   TestProg("\\Google\\Chrome\\Application\\chrome.exe", true);
#endif
#ifdef R__MACOSX
   TestProg("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome");
#endif
#ifdef R__LINUX
   TestProg("/usr/bin/chromium");
   TestProg("/usr/bin/chromium-browser");
   TestProg("/usr/bin/chrome-browser");
#endif

#ifdef _MSC_VER
   fBatchExec = gEnv->GetValue("WebGui.ChromeBatch", "fork: --headless --disable-gpu $url");
   fExec = gEnv->GetValue("WebGui.ChromeInteractive", "$prog --window-size=$width,$height --app=$url");
#else
   fBatchExec = gEnv->GetValue("WebGui.ChromeBatch", "fork:--headless $url");
   fExec = gEnv->GetValue("WebGui.ChromeInteractive", "$prog --window-size=$width,$height --app=\'$url\' &");
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Constructor

ROOT::Experimental::RWebDisplayHandle::FirefoxCreator::FirefoxCreator() : BrowserCreator(true)
{
   TestProg(gEnv->GetValue("WebGui.Firefox", ""));

#ifdef _MSC_VER
   TestProg("\\Mozilla Firefox\\firefox.exe", true);
#endif
#ifdef R__MACOSX
   TestProg("/Applications/Firefox.app/Contents/MacOS/firefox");
#endif
#ifdef R__LINUX
   TestProg("/usr/bin/firefox");
#endif

#ifdef _MSC_VER
   // there is a problem when specifying the window size with wmic on windows:
   // It gives: Invalid format. Hint: <paramlist> = <param> [, <paramlist>].
   fBatchExec = gEnv->GetValue("WebGui.FirefoxBatch", "fork: -headless -no-remote $profile $url");
   fExec = gEnv->GetValue("WebGui.FirefoxInteractive", "$prog -width=$width -height=$height $profile $url");
#else
   fBatchExec = gEnv->GetValue("WebGui.FirefoxBatch", "fork:-headless -no-remote $profile $url");
   fExec = gEnv->GetValue("WebGui.FirefoxInteractive", "$prog -width $width -height $height $profile \'$url\' &");
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Create Firefox profile to run independent browser window

std::string ROOT::Experimental::RWebDisplayHandle::FirefoxCreator::MakeProfile(std::string &exec, bool batch_mode)
{
   std::string rmdir;

   if (exec.find("$profile") == std::string::npos)
      return rmdir;

   std::string profile_arg;

   const char *ff_profile = gEnv->GetValue("WebGui.FirefoxProfile", "");
   const char *ff_profilepath = gEnv->GetValue("WebGui.FirefoxProfilePath", "");
   Int_t ff_randomprofile = gEnv->GetValue("WebGui.FirefoxRandomProfile", (Int_t) 0);
   if (ff_profile && *ff_profile) {
      profile_arg = "-P "s + ff_profile;
   } else if (ff_profilepath && *ff_profilepath) {
      profile_arg = "-profile "s + ff_profilepath;
   } else if ((ff_randomprofile > 0) || (batch_mode && (ff_randomprofile >= 0))) {

      gRandom->SetSeed(0);

      std::string rnd_profile = "root_ff_profile_"s + std::to_string(gRandom->Integer(0x100000));
      std::string profile_dir = std::string(gSystem->TempDirectory()) + "/"s + rnd_profile;

      profile_arg = "-profile "s + profile_dir;
      if (!batch_mode)
         profile_arg = "-no-remote "s + profile_arg;

      if (!fProg.empty()) {
         gSystem->Exec(Form("%s %s -no-remote -CreateProfile \"%s %s\"", fProg.c_str(), (batch_mode ? "-headless" : ""),
                            rnd_profile.c_str(), profile_dir.c_str()));

         rmdir = profile_dir;
      } else {
         R__ERROR_HERE("WebDisplay") << "Cannot create Firefox profile without assigned executable, check WebGui.Firefox variable";
      }
   }

   exec = std::regex_replace(exec, std::regex("\\$profile"), profile_arg);

   return rmdir;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
/// Create web display
/// \param args - defines where and how to display web window
/// Returns RWebDisplayHandle, which holds information of running browser application
/// Can be used fully independent from RWebWindow classes just to show any web page

std::unique_ptr<ROOT::Experimental::RWebDisplayHandle> ROOT::Experimental::RWebDisplayHandle::Display(const RWebDisplayArgs &args)
{
   std::unique_ptr<RWebDisplayHandle> handle;

   auto try_creator = [&](std::unique_ptr<Creator> &creator) {
      if (!creator || !creator->IsActive())
         return false;
      handle = creator->Display(args);
      return handle ? true : false;
   };

   if ((args.GetBrowserKind() == RWebDisplayArgs::kLocal) || (args.GetBrowserKind() == RWebDisplayArgs::kCEF)) {
      if (try_creator(FindCreator("cef", "libROOTCefDisplay")))
         return handle;
   }

   if ((args.GetBrowserKind() == RWebDisplayArgs::kLocal) || (args.GetBrowserKind() == RWebDisplayArgs::kQt5)) {
      if (try_creator(FindCreator("qt5", "libROOTQt5WebDisplay")))
         return handle;
   }

   if (args.IsLocalDisplay()) {
      R__ERROR_HERE("WebDisplay") << "Neither Qt5 nor CEF libraries were found to provide local display";
      return handle;
   }

   if ((args.GetBrowserKind() == RWebDisplayArgs::kNative) || (args.GetBrowserKind() == RWebDisplayArgs::kChrome)) {
      if (try_creator(FindCreator("chrome", "ChromeCreator")))
         return handle;
   }

   if ((args.GetBrowserKind() == RWebDisplayArgs::kNative) || (args.GetBrowserKind() == RWebDisplayArgs::kFirefox)) {
      if (try_creator(FindCreator("firefox", "FirefoxCreator")))
         return handle;
   }

   if ((args.GetBrowserKind() == RWebDisplayArgs::kChrome) || (args.GetBrowserKind() == RWebDisplayArgs::kFirefox)) {
      R__ERROR_HERE("WebDisplay") << "Neither Chrome nor Firefox browser cannot be started to provide display";
      return handle;
   }

   if ((args.GetBrowserKind() == RWebDisplayArgs::kCustom)) {
      std::unique_ptr<Creator> creator = std::make_unique<BrowserCreator>(false, args.GetCustomExec());
      try_creator(creator);
   } else {
      try_creator(FindCreator("browser", "BrowserCreator"));
   }

   return handle;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Display provided url in configured web browser
/// \param url - specified URL address like https://root.cern
/// Browser can specified when starting `root --web=firefox`
/// Returns true when browser started
/// It is convenience method, equivalent to:
///  ~~~
///     RWebDisplayArgs args;
///     args.SetUrl(url);
///     args.SetStandalone(false);
///     auto handle = RWebDisplayHandle::Display(args);
/// ~~~

bool ROOT::Experimental::RWebDisplayHandle::DisplayUrl(const std::string &url)
{
   RWebDisplayArgs args;
   args.SetUrl(url);
   args.SetStandalone(false);

   auto handle = Display(args);

   return !!handle;
}

/*
 * Copyright (c) 2012 Aldebaran Robotics. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the COPYING file.
 */

#include "sdklayout.hpp"
#include "filesystem.hpp"

#include <qi/application.hpp>
#include <qi/path.hpp>
#include <qi/os.hpp>
#include <qi/qi.hpp>
#include <qi/log.hpp>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

qiLogCategory("qi.path");

namespace qi
{
  SDKLayout* gInstance = NULL;
  SDKLayout* getInstance();

  //only needed to make valgrind happy.
  static void cleanupSdkLayout() {
    if (!qi::os::getenv("VALGRIND").empty()) {
      delete gInstance;
      gInstance = 0;
    }
  }

  QI_AT_EXIT(&cleanupSdkLayout);

  namespace path
  {

    boost::filesystem::path absPath(const std::string &pathString)
    {
      boost::filesystem::path path(pathString, qi::unicodeFacet());
      return boost::filesystem::system_complete(path).make_preferred();
    }


    namespace detail {

      void addOptionalSdkPrefix(const char *prefix)
      {
        return getInstance()->addOptionalSdkPrefix(prefix);
      }

      void clearOptionalSdkPrefix()
      {
        return getInstance()->clearOptionalSdkPrefix();
      }


      std::vector<std::string> getSdkPrefixes()
      {
        return getInstance()->getSdkPrefixes();
      }
    }

    std::string sdkPrefix()
    {
      return getInstance()->sdkPrefix();
    }

    std::string findBin(const std::string &name)
    {
      return getInstance()->findBin(name);
    }

    std::string findLib(const std::string &name)
    {
      return getInstance()->findLib(name);
    }

    std::string findConf(const std::string &applicationName,
                         const std::string &filename)
    {
      if(filename == "") {
        qiLogError() << "Filename cannot be empty!";
        return std::string();
      }
      return getInstance()->findConf(applicationName, filename);
    }

    std::string findData(const std::string &applicationName,
                         const std::string &filename)
    {
      if(filename == "") {
        qiLogError() << "Filename cannot be empty!";
        return std::string();
      }
      return getInstance()->findData(applicationName, filename);
    }

    std::vector<std::string> listData(const std::string &applicationName,
                                      const std::string &pattern)
    {
      return getInstance()->listData(applicationName, pattern);
    }

    std::vector<std::string> confPaths(const std::string &applicationName)
    {
      return getInstance()->confPaths(applicationName);
    }

    std::vector<std::string> dataPaths(const std::string &applicationName)
    {
      return getInstance()->dataPaths(applicationName);
    }

    std::vector<std::string> binPaths()
    {
      return getInstance()->binPaths();
    }

    std::vector<std::string> libPaths()
    {
      return getInstance()->libPaths();
    }

    void setWritablePath(const std::string &path)
    {
      qiLogVerbose() << "Writable path set to " << path;
      getInstance()->setWritablePath(path);
    }

    std::string userWritableDataPath(const std::string &applicationName,
                                     const std::string &filename)
    {
      return getInstance()->userWritableDataPath(applicationName, filename);
    }

    std::string userWritableConfPath(const std::string &applicationName,
                                     const std::string &filename)
    {
      return qi::getInstance()->userWritableConfPath(applicationName, filename);
    }
  };

  SDKLayout* getInstance()
  {
    if (gInstance == NULL) {
      std::string prefix;
      std::string mode;
      const char *program = qi::Application::program();

      if (!program) {
        mode = "error";
      }
      else {
        boost::filesystem::path execPath(program, qi::unicodeFacet());
        if(!boost::filesystem::exists(execPath)) {
          mode = "error";
        }
        else {
          execPath = boost::filesystem::system_complete(execPath).make_preferred();
          prefix = execPath.parent_path().parent_path().string(qi::unicodeFacet());
          if (execPath.parent_path().filename().string(qi::unicodeFacet()) != "bin")
            mode = execPath.parent_path().filename().string(qi::unicodeFacet());
          else
            mode = "";
        }
      }
      gInstance = new SDKLayout(prefix, mode);
    }

    return gInstance;
  }

  namespace {
    _QI_COMMAND_LINE_OPTIONS(
      "Chrooting",
      ("writable-path", value<std::string>()->notifier(&qi::path::setWritablePath), "Set the writable path.")
      )
  }
}

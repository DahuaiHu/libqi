﻿/*
**
** Author(s):
**  - Pierre Roullon <proullon@aldebaran-robotics.com>
**
** Copyright (C) 2012 Aldebaran Robotics
*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;

namespace QiMessaging
{
    public class Application
    {
        public Application(string[] args)
        {
            _p = new ApplicationPrivate(args);
        }

        public void Run()
        {
            _p.Run();
        }

        private ApplicationPrivate _p;
    }

    unsafe class ApplicationPrivate
    {
        [DllImport("qimessaging.dll")]
        public static extern qi_application_t* qi_application_create(int *argc, char **argv);

        [DllImport("qimessaging.dll")]
        public static extern void qi_application_stop(qi_application_t* app);

        [DllImport("qimessaging.dll")]
        public static extern void qi_application_run(qi_application_t* app);

        public ApplicationPrivate(string[] args)
        {
            int ac = 0;//args.Length;
            //char** argv = Convertor.ToCharPtr(args);
            app = qi_application_create(&ac, null);
            //args = QiMessaging.Convertor.ToDotNet(ac, argv);
        }

        public void Run()
        {
            qi_application_run(app);
        }

        ~ApplicationPrivate()
        {
            qi_application_stop(app);
        }

        private qi_application_t* app;
    }
}

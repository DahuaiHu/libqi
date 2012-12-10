﻿/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

using System.Runtime.InteropServices;

namespace qi
{
    namespace Messaging
    {
        public class Buffer : SafeBuffer
        {
            public Buffer(bool ownsHandle = true)
                : base(ownsHandle)
            {
            }

            protected override bool ReleaseHandle()
            {
                return true;
            }
        }
    }
}

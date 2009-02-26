// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.

#include "StdAfx.h"
#include "StlDotNet.h"
#include "TemplateRegistry.h"
#include "StlDotNet.h"
#include <streambuf>
#include <istream>
using namespace System;

namespace QuickFASTDotNet {
  namespace Codecs{

    TemplateRegistry^ TemplateRegistry::Parse(System::IO::Stream^ xmlStream)
    {
      QuickFAST::Codecs::XMLTemplateParser templateParser;
      StlDotNet::StreamBuf xmlStreamBuf(xmlStream);
      std::istream xmlDataStream(&xmlStreamBuf);

      return gcnew TemplateRegistry(templateParser.parse(xmlDataStream));
    }
  }

}
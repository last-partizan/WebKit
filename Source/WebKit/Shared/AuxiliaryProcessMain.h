/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if !PLATFORM(COCOA)

#include "AuxiliaryProcess.h"
#include "WebKit2Initialize.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/RuntimeApplicationChecks.h>

namespace WebKit {

class AuxiliaryProcessMainCommon {
public:
    AuxiliaryProcessMainCommon();
    bool parseCommandLine(int argc, char** argv);

protected:
    AuxiliaryProcessInitializationParameters m_parameters;
};

template<typename AuxiliaryProcessType, bool HasSingleton = true>
class AuxiliaryProcessMainBase : public AuxiliaryProcessMainCommon {
public:
    virtual bool platformInitialize() { return true; }
    virtual void platformFinalize() { }

    virtual void initializeAuxiliaryProcess(AuxiliaryProcessInitializationParameters&& parameters)
    {
        if constexpr (HasSingleton)
            AuxiliaryProcessType::singleton().initialize(WTFMove(parameters));
    }

    int run(int argc, char** argv)
    {
        // setAuxiliaryProcessType() should be called before we construct
        // and initialize the AuxiliaryProcess. This is so isInXXXProcess()
        // checks are valid.
        m_parameters.processType = AuxiliaryProcessType::processType;
        setAuxiliaryProcessType(m_parameters.processType);

        if (!platformInitialize())
            return EXIT_FAILURE;

        if (!parseCommandLine(argc, argv))
            return EXIT_FAILURE;

        InitializeWebKit2();

        initializeAuxiliaryProcess(WTFMove(m_parameters));
        RunLoop::run();
        platformFinalize();

        return EXIT_SUCCESS;
    }
};

template<typename AuxiliaryProcessType>
class AuxiliaryProcessMainBaseNoSingleton : public AuxiliaryProcessMainBase<AuxiliaryProcessType, false> {
public:
    AuxiliaryProcessType& process() { return *m_process; };

    void initializeAuxiliaryProcess(AuxiliaryProcessInitializationParameters&& parameters) override
    {
        m_process = adoptRef(new AuxiliaryProcessType(WTFMove(parameters)));
    }

protected:
    RefPtr<AuxiliaryProcessType> m_process;
};

template<typename AuxiliaryProcessMainType>
int AuxiliaryProcessMain(int argc, char** argv)
{
    NeverDestroyed<AuxiliaryProcessMainType> auxiliaryMain;

    return auxiliaryMain->run(argc, argv);
}

} // namespace WebKit

#endif // !PLATFORM(COCOA)

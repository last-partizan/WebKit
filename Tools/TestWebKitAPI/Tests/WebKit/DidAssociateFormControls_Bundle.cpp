/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"

#if WK_HAVE_C_SPI

#include "InjectedBundleTest.h"

#include "PlatformUtilities.h"
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundlePage.h>

namespace TestWebKitAPI {

class DidAssociateFormControlsTest : public InjectedBundleTest {
public:
    DidAssociateFormControlsTest(const std::string& identifier);

    void didCreatePage(WKBundleRef, WKBundlePageRef) override;
};

static InjectedBundleTest::Register<DidAssociateFormControlsTest> registrar("DidAssociateFormControlsTest");

static bool shouldNotifyOnFormChanges(WKBundlePageRef, const void*)
{
    return true;
}

static void didAssociateFormControls(WKBundlePageRef page, WKArrayRef elementHandles, const void*)
{
    WKRetainPtr<WKMutableDictionaryRef> messageBody = adoptWK(WKMutableDictionaryCreate());

    WKDictionarySetItem(messageBody.get(), Util::toWK("Page").get(), page);
    WKRetainPtr<WKUInt64Ref> numberOfElements = adoptWK(WKUInt64Create(WKArrayGetSize(elementHandles)));
    WKDictionarySetItem(messageBody.get(), Util::toWK("NumberOfControls").get(), numberOfElements.get());

    WKBundlePostMessage(InjectedBundleController::singleton().bundle(), Util::toWK("DidReceiveDidAssociateFormControls").get(), messageBody.get());
}

DidAssociateFormControlsTest::DidAssociateFormControlsTest(const std::string& identifier)
    : InjectedBundleTest(identifier)
{
}

void DidAssociateFormControlsTest::didCreatePage(WKBundleRef bundle, WKBundlePageRef page)
{
    WKBundlePageFormClientV2 formClient;
    zeroBytes(formClient);

    formClient.base.version = 2;
    formClient.base.clientInfo = this;
    formClient.shouldNotifyOnFormChanges = shouldNotifyOnFormChanges;
    formClient.didAssociateFormControls = didAssociateFormControls;

    WKBundlePageSetFormClient(page, &formClient.base);
}

} // namespace TestWebKitAPI

#endif

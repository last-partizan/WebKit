/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/codec/SkSampledCodec.h"

#include "include/codec/SkCodec.h"
#include "include/codec/SkEncodedImageFormat.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkRect.h"
#include "include/core/SkTypes.h"
#include "include/private/base/SkTemplates.h"
#include "src/base/SkMathPriv.h"
#include "src/codec/SkCodecPriv.h"
#include "src/codec/SkSampler.h"

SkSampledCodec::SkSampledCodec(SkCodec* codec)
    : INHERITED(codec)
{}

SkISize SkSampledCodec::accountForNativeScaling(int* sampleSizePtr, int* nativeSampleSize) const {
    SkISize preSampledSize = this->codec()->dimensions();
    int sampleSize = *sampleSizePtr;
    SkASSERT(sampleSize > 1);

    if (nativeSampleSize) {
        *nativeSampleSize = 1;
    }

    // Only JPEG supports native downsampling.
    if (this->codec()->getEncodedFormat() == SkEncodedImageFormat::kJPEG) {
        // See if libjpeg supports this scale directly
        switch (sampleSize) {
            case 2:
            case 4:
            case 8:
                // This class does not need to do any sampling.
                *sampleSizePtr = 1;
                return this->codec()->getScaledDimensions(
                        SkCodecPriv::GetScaleFromSampleSize(sampleSize));
            default:
                break;
        }

        // Check if sampleSize is a multiple of something libjpeg can support.
        int remainder;
        const int sampleSizes[] = { 8, 4, 2 };
        for (int supportedSampleSize : sampleSizes) {
            int actualSampleSize;
            SkTDivMod(sampleSize, supportedSampleSize, &actualSampleSize, &remainder);
            if (0 == remainder) {
                float scale = SkCodecPriv::GetScaleFromSampleSize(supportedSampleSize);

                // this->codec() will scale to this size.
                preSampledSize = this->codec()->getScaledDimensions(scale);

                // And then this class will sample it.
                *sampleSizePtr = actualSampleSize;
                if (nativeSampleSize) {
                    *nativeSampleSize = supportedSampleSize;
                }
                break;
            }
        }
    }

    return preSampledSize;
}

SkISize SkSampledCodec::onGetSampledDimensions(int sampleSize) const {
    const SkISize size = this->accountForNativeScaling(&sampleSize);
    return SkISize::Make(SkCodecPriv::GetSampledDimension(size.width(), sampleSize),
                         SkCodecPriv::GetSampledDimension(size.height(), sampleSize));
}

SkCodec::Result SkSampledCodec::onGetAndroidPixels(const SkImageInfo& info, void* pixels,
        size_t rowBytes, const AndroidOptions& options) {
    const SkIRect* subset = options.fSubset;
    if (!subset || subset->size() == this->codec()->dimensions()) {
        if (this->codec()->dimensionsSupported(info.dimensions())) {
            return this->codec()->getPixels(info, pixels, rowBytes, &options);
        }

        // If the native codec does not support the requested scale, scale by sampling.
        return this->sampledDecode(info, pixels, rowBytes, options);
    }

    // We are performing a subset decode.
    int sampleSize = options.fSampleSize;
    SkISize scaledSize = this->getSampledDimensions(sampleSize);
    if (!this->codec()->dimensionsSupported(scaledSize)) {
        // If the native codec does not support the requested scale, scale by sampling.
        return this->sampledDecode(info, pixels, rowBytes, options);
    }

    // Calculate the scaled subset bounds.
    int scaledSubsetX = subset->x() / sampleSize;
    int scaledSubsetY = subset->y() / sampleSize;
    int scaledSubsetWidth = info.width();
    int scaledSubsetHeight = info.height();

    const SkImageInfo scaledInfo = info.makeDimensions(scaledSize);

    // Copy so we can use a different fSubset.
    AndroidOptions subsetOptions = options;
    {
        // Although startScanlineDecode expects the bottom and top to match the
        // SkImageInfo, startIncrementalDecode uses them to determine which rows to
        // decode.
        SkIRect incrementalSubset = SkIRect::MakeXYWH(scaledSubsetX, scaledSubsetY,
                                                      scaledSubsetWidth, scaledSubsetHeight);
        subsetOptions.fSubset = &incrementalSubset;
        const SkCodec::Result startResult = this->codec()->startIncrementalDecode(
                scaledInfo, pixels, rowBytes, &subsetOptions);
        if (SkCodec::kSuccess == startResult) {
            int rowsDecoded = 0;
            const SkCodec::Result incResult = this->codec()->incrementalDecode(&rowsDecoded);
            if (incResult == SkCodec::kSuccess) {
                return SkCodec::kSuccess;
            }
            SkASSERT(incResult == SkCodec::kIncompleteInput || incResult == SkCodec::kErrorInInput);

            // FIXME: Can zero initialized be read from SkCodec::fOptions?
            this->codec()->fillIncompleteImage(scaledInfo, pixels, rowBytes,
                    options.fZeroInitialized, scaledSubsetHeight, rowsDecoded);
            return incResult;
        } else if (startResult != SkCodec::kUnimplemented) {
            return startResult;
        }
        // Otherwise fall down to use the old scanline decoder.
        // subsetOptions.fSubset will be reset below, so it will not continue to
        // point to the object that is no longer on the stack.
    }

    // Start the scanline decode.
    SkIRect scanlineSubset = SkIRect::MakeXYWH(scaledSubsetX, 0, scaledSubsetWidth,
            scaledSize.height());
    subsetOptions.fSubset = &scanlineSubset;

    SkCodec::Result result = this->codec()->startScanlineDecode(scaledInfo,
            &subsetOptions);
    if (SkCodec::kSuccess != result) {
        return result;
    }

    // At this point, we are only concerned with subsetting.  Either no scale was
    // requested, or the this->codec() is handling the scale.
    // Note that subsetting is only supported for kTopDown, so this code will not be
    // reached for other orders.
    SkASSERT(this->codec()->getScanlineOrder() == SkCodec::kTopDown_SkScanlineOrder);
    if (!this->codec()->skipScanlines(scaledSubsetY)) {
        this->codec()->fillIncompleteImage(info, pixels, rowBytes, options.fZeroInitialized,
                scaledSubsetHeight, 0);
        return SkCodec::kIncompleteInput;
    }

    int decodedLines = this->codec()->getScanlines(pixels, scaledSubsetHeight, rowBytes);
    if (decodedLines != scaledSubsetHeight) {
        return SkCodec::kIncompleteInput;
    }
    return SkCodec::kSuccess;
}


SkCodec::Result SkSampledCodec::sampledDecode(const SkImageInfo& info, void* pixels,
        size_t rowBytes, const AndroidOptions& options) {
    // We should only call this function when sampling.
    SkASSERT(options.fSampleSize > 1);

    // FIXME: This was already called by onGetAndroidPixels. Can we reduce that?
    int sampleSize = options.fSampleSize;
    int nativeSampleSize;
    SkISize nativeSize = this->accountForNativeScaling(&sampleSize, &nativeSampleSize);

    // Check if there is a subset.
    SkIRect subset;
    int subsetY = 0;
    int subsetWidth = nativeSize.width();
    int subsetHeight = nativeSize.height();
    if (options.fSubset) {
        // We will need to know about subsetting in the y-dimension in order to use the
        // scanline decoder.
        // Update the subset to account for scaling done by this->codec().
        const SkIRect* subsetPtr = options.fSubset;

        // Do the divide ourselves, instead of calling SkCodecPriv::GetSampledDimension. If
        // X and Y are 0, they should remain 0, rather than being upgraded to 1
        // due to being smaller than the sampleSize.
        const int subsetX = subsetPtr->x() / nativeSampleSize;
        subsetY = subsetPtr->y() / nativeSampleSize;

        subsetWidth = SkCodecPriv::GetSampledDimension(subsetPtr->width(), nativeSampleSize);
        subsetHeight = SkCodecPriv::GetSampledDimension(subsetPtr->height(), nativeSampleSize);

        // The scanline decoder only needs to be aware of subsetting in the x-dimension.
        subset.setXYWH(subsetX, 0, subsetWidth, nativeSize.height());
    }

    // Since we guarantee that output dimensions are always at least one (even if the sampleSize
    // is greater than a given dimension), the input sampleSize is not always the sampleSize that
    // we use in practice.
    const int sampleX = subsetWidth / info.width();
    const int sampleY = subsetHeight / info.height();

    const int samplingOffsetY = SkCodecPriv::GetStartCoord(sampleY);
    const int startY = samplingOffsetY + subsetY;
    const int dstHeight = info.height();

    const SkImageInfo nativeInfo = info.makeDimensions(nativeSize);

    {
        // Although startScanlineDecode expects the bottom and top to match the
        // SkImageInfo, startIncrementalDecode uses them to determine which rows to
        // decode.
        AndroidOptions incrementalOptions = options;
        SkIRect incrementalSubset;
        if (options.fSubset) {
            incrementalSubset.fTop     = subsetY;
            incrementalSubset.fBottom  = subsetY + subsetHeight;
            incrementalSubset.fLeft    = subset.fLeft;
            incrementalSubset.fRight   = subset.fRight;
            incrementalOptions.fSubset = &incrementalSubset;
        }
        const SkCodec::Result startResult = this->codec()->startIncrementalDecode(nativeInfo,
                pixels, rowBytes, &incrementalOptions);
        if (SkCodec::kSuccess == startResult) {
            SkSampler* sampler = this->codec()->getSampler(true);
            if (!sampler) {
                return SkCodec::kUnimplemented;
            }

            if (sampler->setSampleX(sampleX) != info.width()) {
                return SkCodec::kInvalidScale;
            }
            if (SkCodecPriv::GetSampledDimension(subsetHeight, sampleY) != info.height()) {
                return SkCodec::kInvalidScale;
            }

            sampler->setSampleY(sampleY);

            int rowsDecoded = 0;
            const SkCodec::Result incResult = this->codec()->incrementalDecode(&rowsDecoded);
            if (incResult == SkCodec::kSuccess) {
                return SkCodec::kSuccess;
            }
            SkASSERT(incResult == SkCodec::kIncompleteInput || incResult == SkCodec::kErrorInInput);

            SkASSERT(rowsDecoded <= info.height());
            this->codec()->fillIncompleteImage(info, pixels, rowBytes, options.fZeroInitialized,
                                               info.height(), rowsDecoded);
            return incResult;
        } else if (startResult == SkCodec::kIncompleteInput
                || startResult == SkCodec::kErrorInInput) {
            return SkCodec::kInvalidInput;
        } else if (startResult != SkCodec::kUnimplemented) {
            return startResult;
        } // kUnimplemented means use the old method.
    }

    // Start the scanline decode.
    AndroidOptions sampledOptions = options;
    if (options.fSubset) {
        sampledOptions.fSubset = &subset;
    }
    SkCodec::Result result = this->codec()->startScanlineDecode(nativeInfo,
            &sampledOptions);
    if (SkCodec::kIncompleteInput == result || SkCodec::kErrorInInput == result) {
        return SkCodec::kInvalidInput;
    } else if (SkCodec::kSuccess != result) {
        return result;
    }

    SkSampler* sampler = this->codec()->getSampler(true);
    if (!sampler) {
        return SkCodec::kInternalError;
    }

    if (sampler->setSampleX(sampleX) != info.width()) {
        return SkCodec::kInvalidScale;
    }
    if (SkCodecPriv::GetSampledDimension(subsetHeight, sampleY) != info.height()) {
        return SkCodec::kInvalidScale;
    }

    switch(this->codec()->getScanlineOrder()) {
        case SkCodec::kTopDown_SkScanlineOrder: {
            if (!this->codec()->skipScanlines(startY)) {
                this->codec()->fillIncompleteImage(info, pixels, rowBytes, options.fZeroInitialized,
                        dstHeight, 0);
                return SkCodec::kIncompleteInput;
            }
            void* pixelPtr = pixels;
            for (int y = 0; y < dstHeight; y++) {
                if (1 != this->codec()->getScanlines(pixelPtr, 1, rowBytes)) {
                    this->codec()->fillIncompleteImage(info, pixels, rowBytes,
                            options.fZeroInitialized, dstHeight, y + 1);
                    return SkCodec::kIncompleteInput;
                }
                if (y < dstHeight - 1) {
                    if (!this->codec()->skipScanlines(sampleY - 1)) {
                        this->codec()->fillIncompleteImage(info, pixels, rowBytes,
                                options.fZeroInitialized, dstHeight, y + 1);
                        return SkCodec::kIncompleteInput;
                    }
                }
                pixelPtr = SkTAddOffset<void>(pixelPtr, rowBytes);
            }
            return SkCodec::kSuccess;
        }
        case SkCodec::kBottomUp_SkScanlineOrder: {
            // Note that these modes do not support subsetting.
            SkASSERT(0 == subsetY && nativeSize.height() == subsetHeight);
            int y;
            for (y = 0; y < nativeSize.height(); y++) {
                int srcY = this->codec()->nextScanline();
                if (SkCodecPriv::IsCoordNecessary(srcY, sampleY, dstHeight)) {
                    void* pixelPtr = SkTAddOffset<void>(
                            pixels, rowBytes * SkCodecPriv::GetDstCoord(srcY, sampleY));
                    if (1 != this->codec()->getScanlines(pixelPtr, 1, rowBytes)) {
                        break;
                    }
                } else {
                    if (!this->codec()->skipScanlines(1)) {
                        break;
                    }
                }
            }

            if (nativeSize.height() == y) {
                return SkCodec::kSuccess;
            }

            // We handle filling uninitialized memory here instead of using this->codec().
            // this->codec() does not know that we are sampling.
            const SkImageInfo fillInfo = info.makeWH(info.width(), 1);
            for (; y < nativeSize.height(); y++) {
                int srcY = this->codec()->outputScanline(y);
                if (!SkCodecPriv::IsCoordNecessary(srcY, sampleY, dstHeight)) {
                    continue;
                }

                void* rowPtr = SkTAddOffset<void>(
                        pixels, rowBytes * SkCodecPriv::GetDstCoord(srcY, sampleY));
                SkSampler::Fill(fillInfo, rowPtr, rowBytes, options.fZeroInitialized);
            }
            return SkCodec::kIncompleteInput;
        }
        default:
            SkASSERT(false);
            return SkCodec::kUnimplemented;
    }
}

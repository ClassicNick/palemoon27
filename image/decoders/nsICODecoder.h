/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsICODecoder_h
#define nsICODecoder_h

#include "nsAutoPtr.h"
#include "Decoder.h"
#include "imgFrame.h"
#include "nsBMPDecoder.h"
#include "nsPNGDecoder.h"
#include "ICOFileHeaders.h"

namespace mozilla {
namespace image {

class RasterImage;

class nsICODecoder : public Decoder
{
public:

  explicit nsICODecoder(RasterImage* aImage);
  virtual ~nsICODecoder();

  // Obtains the width of the icon directory entry
  uint32_t GetRealWidth() const
  {
    return mDirEntry.mWidth == 0 ? 256 : mDirEntry.mWidth;
  }

  // Obtains the height of the icon directory entry
  uint32_t GetRealHeight() const
  {
    return mDirEntry.mHeight == 0 ? 256 : mDirEntry.mHeight;
  }

  virtual void WriteInternal(const char* aBuffer, uint32_t aCount) override;
  virtual void FinishInternal() override;
  virtual nsresult AllocateFrame(const nsIntSize& aTargetSize
                                   /* = nsIntSize() */) override;

protected:
  virtual bool NeedsNewFrame() const override;

private:
  // Writes to the contained decoder and sets the appropriate errors
  // Returns true if there are no errors.
  bool WriteToContainedDecoder(const char* aBuffer, uint32_t aCount);

  // Processes a single dir entry of the icon resource
  void ProcessDirEntry(IconDirEntry& aTarget);
  // Sets the hotspot property of if we have a cursor
  void SetHotSpotIfCursor();
  // Creates a bitmap file header buffer, returns true if successful
  bool FillBitmapFileHeaderBuffer(int8_t *bfh);
  /**
   * Verifies that the width and height values in @aBIH are valid and match the
   * values we read from the ICO directory entry. If everything looks OK, the
   * height value in @aBIH is updated to compensate for the AND mask, which the
   * underlying BMP decoder doesn't know about.
   *
   * @return true if the width and height values in @aBIH are valid and correct.
   */
  bool CheckAndFixBitmapSize(int8_t* aBIH);
  // Extract bitmap info header size count from BMP information header
  int32_t ExtractBIHSizeFromBitmap(int8_t *bih);
  // Extract bit count from BMP information header
  int32_t ExtractBPPFromBitmap(int8_t *bih);
  // Calculates the row size in bytes for the AND mask table
  uint32_t CalcAlphaRowSize();
  // Obtains the number of colors from the BPP, mBPP must be filled in
  uint16_t GetNumColors();

  uint16_t mBPP; // Stores the images BPP
  uint32_t mPos; // Keeps track of the position we have decoded up until
  uint16_t mNumIcons; // Stores the number of icons in the ICO file
  uint16_t mCurrIcon; // Stores the current dir entry index we are processing
  uint32_t mImageOffset; // Stores the offset of the image data we want
  uint8_t* mRow;      // Holds one raw line of the image
  int32_t mCurLine;   // Line index of the image that's currently being decoded
  uint32_t mRowBytes; // How many bytes of the row were already received
  int32_t mOldLine;   // Previous index of the line
  nsRefPtr<Decoder> mContainedDecoder; // Contains either a BMP or PNG resource
  RawAccessFrameRef mRefForContainedDecoder; // Avoid locking off-main-thread

  char mDirEntryArray[ICODIRENTRYSIZE]; // Holds the current dir entry buffer
  IconDirEntry mDirEntry; // Holds a decoded dir entry
  // Holds the potential bytes that can be a PNG signature
  char mSignature[PNGSIGNATURESIZE];
  // Holds the potential bytes for a bitmap information header
  char mBIHraw[40];
  // Stores whether or not the icon file we are processing has type 1 (icon)
  bool mIsCursor;
  // Stores whether or not the contained resource is a PNG
  bool mIsPNG;
};

} // namespace image
} // namespace mozilla

#endif // nsICODecoder_h

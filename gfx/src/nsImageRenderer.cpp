/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "libimg.h"
#include "ilIImageRenderer.h"
#include "nsIImage.h"
#include "nsIRenderingContext.h"
#include "ni_pixmp.h"
#include "il_util.h"
#include "nsGfxCIID.h"
#include "nsIDeviceContext.h"

static NS_DEFINE_IID(kIImageRendererIID, IL_IIMAGERENDERER_IID);

class ImageRendererImpl : public ilIImageRenderer {
public:
  ImageRendererImpl();
  virtual ~ImageRendererImpl() { }

  NS_DECL_ISUPPORTS

  virtual void NewPixmap(void* aDisplayContext, 
			                   PRInt32 aWidth, PRInt32 aHeight, 
			                   IL_Pixmap* aImage, IL_Pixmap* aMask);

  virtual void UpdatePixmap(void* aDisplayContext, 
			                      IL_Pixmap* aImage, 
			                      PRInt32 aXOffset, PRInt32 aYOffset, 
			                      PRInt32 aWidth, PRInt32 aHeight);

  virtual void ControlPixmapBits(void* aDisplayContext, 
				                         IL_Pixmap* aImage, PRUint32 aControlMsg);

  virtual void DestroyPixmap(void* aDisplayContext, IL_Pixmap* aImage);
  
  virtual void DisplayPixmap(void* aDisplayContext, 
                  			     IL_Pixmap* aImage, IL_Pixmap* aMask, 
                  			     PRInt32 aX, PRInt32 aY, 
                  			     PRInt32 aXOffset, PRInt32 aYOffset, 
                  			     PRInt32 aWidth, PRInt32 aHeight);

  virtual void DisplayIcon(void* aDisplayContext, 
			                     PRInt32 aX, PRInt32 aY, PRUint32 aIconNumber);

  virtual void GetIconDimensions(void* aDisplayContext, 
                        				 PRInt32 *aWidthPtr, PRInt32 *aHeightPtr, 
                        				 PRUint32 aIconNumber);
};

ImageRendererImpl::ImageRendererImpl()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(ImageRendererImpl, kIImageRendererIID)

void 
ImageRendererImpl::NewPixmap(void* aDisplayContext, 
			                       PRInt32 aWidth, PRInt32 aHeight, 
                      	     IL_Pixmap* aImage, IL_Pixmap* aMask)
{
  nsIDeviceContext *dc = (nsIDeviceContext *)aDisplayContext;
  nsIImage  *img;
  nsresult  rv;

  static NS_DEFINE_IID(kImageCID, NS_IMAGE_CID);
  static NS_DEFINE_IID(kImageIID, NS_IIMAGE_IID);

  // Create a new image object
  rv = nsRepository::CreateInstance(kImageCID, nsnull, kImageIID, (void **)&img);
  if (NS_OK != rv) {
    // XXX What about error handling?
    return;
  }

  // Have the image match the depth and color space associated with the
  // device.
  // XXX We probably don't want to do that for monomchrome images (e.g., XBM)
  // or one-bit deep GIF images.
  PRInt32 depth;
  IL_ColorSpace *colorSpace;

  dc->GetILColorSpace(colorSpace);
  depth = colorSpace->pixmap_depth;

  // Initialize the image object
  img->Init(aWidth, aHeight, depth, (aMask == nsnull) ? nsMaskRequirements_kNoMask : 
	          nsMaskRequirements_kNeeds1Bit);

  // Update the pixmap image and mask information
  aImage->bits = img->GetBits();
  aImage->client_data = img;  // we don't need to add a ref here, because there's
                              // already one from the call to create the image object
  aImage->header.width = aWidth;
  aImage->header.height = aHeight;
  aImage->header.widthBytes = img->GetLineStride();

  if (aMask) {
    aMask->bits = img->GetAlphaBits();
    aMask->client_data = img;
    // We must add another reference here, because when the mask's pixmap is
    // destroyed it will release a reference
    NS_ADDREF(img);
    aMask->header.width = aWidth;
    aMask->header.height = aHeight;
  }

  // Replace the existing color space with the color space associated
  // with the device.
  IL_ReleaseColorSpace(aImage->header.color_space);
  aImage->header.color_space = colorSpace;

  // XXX Why do we do this on a per-image basis?
  if (8 == depth) {
    IL_ColorMap *cmap = &colorSpace->cmap;
    nsColorMap *nscmap = img->GetColorMap();
    PRUint8 *mapptr = nscmap->Index;
    int i;
                
    for (i=0; i < cmap->num_colors; i++) {
      *mapptr++ = cmap->map[i].red;
      *mapptr++ = cmap->map[i].green;
      *mapptr++ = cmap->map[i].blue;
    }

    img->ImageUpdated(dc, nsImageUpdateFlags_kColorMapChanged, nsnull);
                
    if (aImage->header.transparent_pixel) {
      PRUint8 red, green, blue;
      PRUint8 *lookup_table = (PRUint8 *)aImage->header.color_space->cmap.table;
      red = aImage->header.transparent_pixel->red;
      green = aImage->header.transparent_pixel->green;
      blue = aImage->header.transparent_pixel->blue;
      aImage->header.transparent_pixel->index = lookup_table[((red >> 3) << 10) |
                                                             ((green >> 3) << 5) |
                                                             (blue >> 3)];
    }
  }
}

void 
ImageRendererImpl::UpdatePixmap(void* aDisplayContext, 
				                        IL_Pixmap* aImage, 
				                        PRInt32 aXOffset, PRInt32 aYOffset, 
				                        PRInt32 aWidth, PRInt32 aHeight)
{
  nsIDeviceContext *dc = (nsIDeviceContext *)aDisplayContext;
  nsIImage         *img = (nsIImage *)aImage->client_data;
  nsRect            drect(aXOffset, aYOffset, aWidth, aHeight);

  img->ImageUpdated(dc, nsImageUpdateFlags_kBitsChanged, &drect);
}

void 
ImageRendererImpl::ControlPixmapBits(void* aDisplayContext, 
				                             IL_Pixmap* aImage, PRUint32 aControlMsg)
{
  nsIDeviceContext *dc = (nsIDeviceContext *)aDisplayContext;
  nsIImage *img = (nsIImage *)aImage->client_data;

  if (aControlMsg == IL_RELEASE_BITS) {
    img->Optimize(dc);
  }
}

void 
ImageRendererImpl::DestroyPixmap(void* aDisplayContext, IL_Pixmap* aImage)
{
  nsIDeviceContext *dc = (nsIDeviceContext *)aDisplayContext;
  nsIImage *img = (nsIImage *)aImage->client_data;

  aImage->client_data = nsnull;
  if (img) {
    NS_RELEASE(img);
  }
}
  
void 
ImageRendererImpl::DisplayPixmap(void* aDisplayContext, 
				                         IL_Pixmap* aImage, IL_Pixmap* aMask, 
                        				 PRInt32 aX, PRInt32 aY, 
                        				 PRInt32 aXOffset, PRInt32 aYOffset, 
                        				 PRInt32 aWidth, PRInt32 aHeight)
{
  // Image library doesn't drive the display process.
  // XXX Why is this part of the API?
}

void 
ImageRendererImpl::DisplayIcon(void* aDisplayContext, 
			                         PRInt32 aX, PRInt32 aY, PRUint32 aIconNumber)
{
  // XXX Why is this part of the API?
}

void 
ImageRendererImpl::GetIconDimensions(void* aDisplayContext, 
				                             PRInt32 *aWidthPtr, PRInt32 *aHeightPtr, 
                        				     PRUint32 aIconNumber)
{
  // XXX Why is this part of the API?
}

extern "C" NS_GFX_(nsresult)
NS_NewImageRenderer(ilIImageRenderer  **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  ilIImageRenderer *renderer = new ImageRendererImpl();
  if (renderer == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return renderer->QueryInterface(kIImageRendererIID, (void **)aInstancePtrResult);
}

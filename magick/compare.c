/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%               CCCC   OOO   M   M  PPPP    AAA   RRRR    EEEEE               %
%              C      O   O  MM MM  P   P  A   A  R   R   E                   %
%              C      O   O  M M M  PPPP   AAAAA  RRRR    EEE                 %
%              C      O   O  M   M  P      A   A  R R     E                   %
%               CCCC   OOO   M   M  P      A   A  R  R    EEEEE               %
%                                                                             %
%                                                                             %
%                      MagickCore Image Comparison Methods                    %
%                                                                             %
%                              Software Design                                %
%                                  Cristy                                     %
%                               December 2003                                 %
%                                                                             %
%                                                                             %
%  Copyright 1999-2018 ImageMagick Studio LLC, a non-profit organization      %
%  dedicated to making software imaging solutions freely available.           %
%                                                                             %
%  You may not use this file except in compliance with the License.  You may  %
%  obtain a copy of the License at                                            %
%                                                                             %
%    https://imagemagick.org/script/license.php                               %
%                                                                             %
%  Unless required by applicable law or agreed to in writing, software        %
%  distributed under the License is distributed on an "AS IS" BASIS,          %
%  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   %
%  See the License for the specific language governing permissions and        %
%  limitations under the License.                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%
%
*/

/*
  Include declarations.
*/
#include "magick/studio.h"
#include "magick/artifact.h"
#include "magick/cache-view.h"
#include "magick/channel.h"
#include "magick/client.h"
#include "magick/color.h"
#include "magick/color-private.h"
#include "magick/colorspace.h"
#include "magick/colorspace-private.h"
#include "magick/compare.h"
#include "magick/composite-private.h"
#include "magick/constitute.h"
#include "magick/exception-private.h"
#include "magick/geometry.h"
#include "magick/image-private.h"
#include "magick/list.h"
#include "magick/log.h"
#include "magick/memory_.h"
#include "magick/monitor.h"
#include "magick/monitor-private.h"
#include "magick/option.h"
#include "magick/pixel-private.h"
#include "magick/property.h"
#include "magick/resource_.h"
#include "magick/string_.h"
#include "magick/string-private.h"
#include "magick/statistic.h"
#include "magick/thread-private.h"
#include "magick/transform.h"
#include "magick/utility.h"
#include "magick/version.h"

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   C o m p a r e I m a g e C h a n n e l s                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  CompareImageChannels() compares one or more image channels of an image
%  to a reconstructed image and returns the difference image.
%
%  The format of the CompareImageChannels method is:
%
%      Image *CompareImageChannels(const Image *image,
%        const Image *reconstruct_image,const ChannelType channel,
%        const MetricType metric,double *distortion,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o reconstruct_image: the reconstruct image.
%
%    o channel: the channel.
%
%    o metric: the metric.
%
%    o distortion: the computed distortion between the images.
%
%    o exception: return any errors or warnings in this structure.
%
*/

MagickExport Image *CompareImages(Image *image,const Image *reconstruct_image,
  const MetricType metric,double *distortion,ExceptionInfo *exception)
{
  Image
    *highlight_image;

  highlight_image=CompareImageChannels(image,reconstruct_image,
    CompositeChannels,metric,distortion,exception);
  return(highlight_image);
}

static size_t GetNumberChannels(const Image *image,const ChannelType channel)
{
  size_t
    channels;

  channels=0;
  if ((channel & RedChannel) != 0)
    channels++;
  if ((channel & GreenChannel) != 0)
    channels++;
  if ((channel & BlueChannel) != 0)
    channels++;
  if (((channel & OpacityChannel) != 0) && (image->matte != MagickFalse))
    channels++;
  if (((channel & IndexChannel) != 0) && (image->colorspace == CMYKColorspace))
    channels++;
  return(channels == 0 ? 1UL : channels);
}

static inline MagickBooleanType ValidateImageMorphology(
  const Image *magick_restrict image,
  const Image *magick_restrict reconstruct_image)
{
  /*
    Does the image match the reconstructed image morphology?
  */
  if (GetNumberChannels(image,DefaultChannels) !=
      GetNumberChannels(reconstruct_image,DefaultChannels))
    return(MagickFalse);
  return(MagickTrue);
}

MagickExport Image *CompareImageChannels(Image *image,
  const Image *reconstruct_image,const ChannelType channel,
  const MetricType metric,double *distortion,ExceptionInfo *exception)
{
  CacheView
    *highlight_view,
    *image_view,
    *reconstruct_view;

  const char
    *artifact;

  double
    fuzz;

  Image
    *clone_image,
    *difference_image,
    *highlight_image;

  MagickBooleanType
    status;

  MagickPixelPacket
    highlight,
    lowlight,
    zero;

  size_t
    columns,
    rows;

  ssize_t
    y;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(reconstruct_image != (const Image *) NULL);
  assert(reconstruct_image->signature == MagickCoreSignature);
  assert(distortion != (double *) NULL);
  *distortion=0.0;
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if (metric != PerceptualHashErrorMetric)
    if (ValidateImageMorphology(image,reconstruct_image) == MagickFalse)
      ThrowImageException(ImageError,"ImageMorphologyDiffers");
  status=GetImageChannelDistortion(image,reconstruct_image,channel,metric,
    distortion,exception);
  if (status == MagickFalse)
    return((Image *) NULL);
  clone_image=CloneImage(image,0,0,MagickTrue,exception);
  if (clone_image == (Image *) NULL)
    return((Image *) NULL);
  (void) SetImageMask(clone_image,(Image *) NULL);
  difference_image=CloneImage(clone_image,0,0,MagickTrue,exception);
  clone_image=DestroyImage(clone_image);
  if (difference_image == (Image *) NULL)
    return((Image *) NULL);
  (void) SetImageAlphaChannel(difference_image,OpaqueAlphaChannel);
  rows=MagickMax(image->rows,reconstruct_image->rows);
  columns=MagickMax(image->columns,reconstruct_image->columns);
  highlight_image=CloneImage(image,columns,rows,MagickTrue,exception);
  if (highlight_image == (Image *) NULL)
    {
      difference_image=DestroyImage(difference_image);
      return((Image *) NULL);
    }
  if (SetImageStorageClass(highlight_image,DirectClass) == MagickFalse)
    {
      InheritException(exception,&highlight_image->exception);
      difference_image=DestroyImage(difference_image);
      highlight_image=DestroyImage(highlight_image);
      return((Image *) NULL);
    }
  (void) SetImageMask(highlight_image,(Image *) NULL);
  (void) SetImageAlphaChannel(highlight_image,OpaqueAlphaChannel);
  (void) QueryMagickColor("#f1001ecc",&highlight,exception);
  artifact=GetImageArtifact(image,"compare:highlight-color");
  if (artifact != (const char *) NULL)
    (void) QueryMagickColor(artifact,&highlight,exception);
  (void) QueryMagickColor("#ffffffcc",&lowlight,exception);
  artifact=GetImageArtifact(image,"compare:lowlight-color");
  if (artifact != (const char *) NULL)
    (void) QueryMagickColor(artifact,&lowlight,exception);
  if (highlight_image->colorspace == CMYKColorspace)
    {
      ConvertRGBToCMYK(&highlight);
      ConvertRGBToCMYK(&lowlight);
    }
  /*
    Generate difference image.
  */
  status=MagickTrue;
  fuzz=GetFuzzyColorDistance(image,reconstruct_image);
  GetMagickPixelPacket(image,&zero);
  image_view=AcquireVirtualCacheView(image,exception);
  reconstruct_view=AcquireVirtualCacheView(reconstruct_image,exception);
  highlight_view=AcquireAuthenticCacheView(highlight_image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(status) \
    magick_number_threads(image,highlight_image,rows,1)
#endif
  for (y=0; y < (ssize_t) rows; y++)
  {
    MagickBooleanType
      sync;

    MagickPixelPacket
      pixel,
      reconstruct_pixel;

    register const IndexPacket
      *magick_restrict indexes,
      *magick_restrict reconstruct_indexes;

    register const PixelPacket
      *magick_restrict p,
      *magick_restrict q;

    register IndexPacket
      *magick_restrict highlight_indexes;

    register PixelPacket
      *magick_restrict r;

    register ssize_t
      x;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,columns,1,exception);
    q=GetCacheViewVirtualPixels(reconstruct_view,0,y,columns,1,exception);
    r=QueueCacheViewAuthenticPixels(highlight_view,0,y,columns,1,exception);
    if ((p == (const PixelPacket *) NULL) ||
        (q == (const PixelPacket *) NULL) || (r == (PixelPacket *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    reconstruct_indexes=GetCacheViewVirtualIndexQueue(reconstruct_view);
    highlight_indexes=GetCacheViewAuthenticIndexQueue(highlight_view);
    pixel=zero;
    reconstruct_pixel=zero;
    for (x=0; x < (ssize_t) columns; x++)
    {
      MagickStatusType
        difference;

      SetMagickPixelPacket(image,p,indexes+x,&pixel);
      SetMagickPixelPacket(reconstruct_image,q,reconstruct_indexes+x,
        &reconstruct_pixel);
      difference=MagickFalse;
      if (channel == CompositeChannels)
        {
          if (IsMagickColorSimilar(&pixel,&reconstruct_pixel) == MagickFalse)
            difference=MagickTrue;
        }
      else
        {
          double
            Da,
            distance,
            Sa;

          Sa=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(p) :
            (QuantumRange-OpaqueOpacity));
          Da=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(q) :
            (QuantumRange-OpaqueOpacity));
          if ((channel & RedChannel) != 0)
            {
              distance=Sa*GetPixelRed(p)-Da*GetPixelRed(q);
              if ((distance*distance) > fuzz)
                difference=MagickTrue;
            }
          if ((channel & GreenChannel) != 0)
            {
              distance=Sa*GetPixelGreen(p)-Da*GetPixelGreen(q);
              if ((distance*distance) > fuzz)
                difference=MagickTrue;
            }
          if ((channel & BlueChannel) != 0)
            {
              distance=Sa*GetPixelBlue(p)-Da*GetPixelBlue(q);
              if ((distance*distance) > fuzz)
                difference=MagickTrue;
            }
          if (((channel & OpacityChannel) != 0) &&
              (image->matte != MagickFalse))
            {
              distance=(double) GetPixelOpacity(p)-GetPixelOpacity(q);
              if ((distance*distance) > fuzz)
                difference=MagickTrue;
            }
          if (((channel & IndexChannel) != 0) &&
              (image->colorspace == CMYKColorspace))
            {
              distance=Sa*indexes[x]-Da*reconstruct_indexes[x];
              if ((distance*distance) > fuzz)
                difference=MagickTrue;
            }
        }
      if (difference != MagickFalse)
        SetPixelPacket(highlight_image,&highlight,r,highlight_indexes+x);
      else
        SetPixelPacket(highlight_image,&lowlight,r,highlight_indexes+x);
      p++;
      q++;
      r++;
    }
    sync=SyncCacheViewAuthenticPixels(highlight_view,exception);
    if (sync == MagickFalse)
      status=MagickFalse;
  }
  highlight_view=DestroyCacheView(highlight_view);
  reconstruct_view=DestroyCacheView(reconstruct_view);
  image_view=DestroyCacheView(image_view);
  (void) CompositeImage(difference_image,image->compose,highlight_image,0,0);
  highlight_image=DestroyImage(highlight_image);
  if (status == MagickFalse)
    difference_image=DestroyImage(difference_image);
  return(difference_image);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   G e t I m a g e C h a n n e l D i s t o r t i o n                         %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  GetImageChannelDistortion() compares one or more image channels of an image
%  to a reconstructed image and returns the specified distortion metric.
%
%  The format of the GetImageChannelDistortion method is:
%
%      MagickBooleanType GetImageChannelDistortion(const Image *image,
%        const Image *reconstruct_image,const ChannelType channel,
%        const MetricType metric,double *distortion,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o reconstruct_image: the reconstruct image.
%
%    o channel: the channel.
%
%    o metric: the metric.
%
%    o distortion: the computed distortion between the images.
%
%    o exception: return any errors or warnings in this structure.
%
*/

MagickExport MagickBooleanType GetImageDistortion(Image *image,
  const Image *reconstruct_image,const MetricType metric,double *distortion,
  ExceptionInfo *exception)
{
  MagickBooleanType
    status;

  status=GetImageChannelDistortion(image,reconstruct_image,CompositeChannels,
    metric,distortion,exception);
  return(status);
}

static MagickBooleanType GetAbsoluteDistortion(const Image *image,
  const Image *reconstruct_image,const ChannelType channel,double *distortion,
  ExceptionInfo *exception)
{
  CacheView
    *image_view,
    *reconstruct_view;

  double
    fuzz;

  MagickBooleanType
    status;

  size_t
    columns,
    rows;

  ssize_t
    y;

  /*
    Compute the absolute difference in pixels between two images.
  */
  status=MagickTrue;
  fuzz=MagickMin(GetNumberChannels(image,channel),
    GetNumberChannels(reconstruct_image,channel))*
    GetFuzzyColorDistance(image,reconstruct_image);
  rows=MagickMax(image->rows,reconstruct_image->rows);
  columns=MagickMax(image->columns,reconstruct_image->columns);
  image_view=AcquireVirtualCacheView(image,exception);
  reconstruct_view=AcquireVirtualCacheView(reconstruct_image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(status) \
    magick_number_threads(image,image,rows,1)
#endif
  for (y=0; y < (ssize_t) rows; y++)
  {
    double
      channel_distortion[CompositeChannels+1];

    register const IndexPacket
      *magick_restrict indexes,
      *magick_restrict reconstruct_indexes;

    register const PixelPacket
      *magick_restrict p,
      *magick_restrict q;

    register ssize_t
      i,
      x;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,columns,1,exception);
    q=GetCacheViewVirtualPixels(reconstruct_view,0,y,columns,1,exception);
    if ((p == (const PixelPacket *) NULL) || (q == (const PixelPacket *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    reconstruct_indexes=GetCacheViewVirtualIndexQueue(reconstruct_view);
    (void) memset(channel_distortion,0,sizeof(channel_distortion));
    for (x=0; x < (ssize_t) columns; x++)
    {
      double
        Da,
        distance,
        pixel,
        Sa;

      MagickBooleanType
        difference;

      difference=MagickFalse;
      Sa=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(p) :
        (QuantumRange-OpaqueOpacity));
      Da=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(q) :
        (QuantumRange-OpaqueOpacity));
      distance=0.0;
      if ((channel & RedChannel) != 0)
        {
          pixel=Sa*GetPixelRed(p)-Da*GetPixelRed(q);
          distance+=pixel*pixel;
          if (distance > fuzz)
            {
              channel_distortion[RedChannel]++;
              difference=MagickTrue;
            }
        }
      if ((channel & GreenChannel) != 0)
        {
          pixel=Sa*GetPixelGreen(p)-Da*GetPixelGreen(q);
          distance+=pixel*pixel;
          if (distance > fuzz)
            {
              channel_distortion[GreenChannel]++;
              difference=MagickTrue;
            }
        }
      if ((channel & BlueChannel) != 0)
        {
          pixel=Sa*GetPixelBlue(p)-Da*GetPixelBlue(q);
          distance+=pixel*pixel;
          if (distance > fuzz)
            {
              channel_distortion[BlueChannel]++;
              difference=MagickTrue;
            }
        }
      if (((channel & OpacityChannel) != 0) &&
          (image->matte != MagickFalse))
        {
          pixel=(double) GetPixelOpacity(p)-GetPixelOpacity(q);
          distance+=pixel*pixel;
          if (distance > fuzz)
            {
              channel_distortion[OpacityChannel]++;
              difference=MagickTrue;
            }
        }
      if (((channel & IndexChannel) != 0) &&
          (image->colorspace == CMYKColorspace))
        {
          pixel=Sa*indexes[x]-Da*reconstruct_indexes[x];
          distance+=pixel*pixel;
          if (distance > fuzz)
            {
              channel_distortion[BlackChannel]++;
              difference=MagickTrue;
            }
        }
      if (difference != MagickFalse)
        channel_distortion[CompositeChannels]++;
      p++;
      q++;
    }
#if defined(MAGICKCORE_OPENMP_SUPPORT)
    #pragma omp critical (MagickCore_GetAbsoluteDistortion)
#endif
    for (i=0; i <= (ssize_t) CompositeChannels; i++)
      distortion[i]+=channel_distortion[i];
  }
  reconstruct_view=DestroyCacheView(reconstruct_view);
  image_view=DestroyCacheView(image_view);
  return(status);
}

static MagickBooleanType GetFuzzDistortion(const Image *image,
  const Image *reconstruct_image,const ChannelType channel,
  double *distortion,ExceptionInfo *exception)
{
  CacheView
    *image_view,
    *reconstruct_view;

  MagickBooleanType
    status;

  register ssize_t
    i;

  size_t
    columns,
    rows;

  ssize_t
    y;

  status=MagickTrue;
  rows=MagickMax(image->rows,reconstruct_image->rows);
  columns=MagickMax(image->columns,reconstruct_image->columns);
  image_view=AcquireVirtualCacheView(image,exception);
  reconstruct_view=AcquireVirtualCacheView(reconstruct_image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(status) \
    magick_number_threads(image,image,rows,1)
#endif
  for (y=0; y < (ssize_t) rows; y++)
  {
    double
      channel_distortion[CompositeChannels+1];

    register const IndexPacket
      *magick_restrict indexes,
      *magick_restrict reconstruct_indexes;

    register const PixelPacket
      *magick_restrict p,
      *magick_restrict q;

    register ssize_t
      i,
      x;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,columns,1,exception);
    q=GetCacheViewVirtualPixels(reconstruct_view,0,y,columns,1,exception);
    if ((p == (const PixelPacket *) NULL) || (q == (const PixelPacket *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    reconstruct_indexes=GetCacheViewVirtualIndexQueue(reconstruct_view);
    (void) memset(channel_distortion,0,sizeof(channel_distortion));
    for (x=0; x < (ssize_t) columns; x++)
    {
      MagickRealType
        distance,
        Da,
        Sa;

      Sa=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(p) :
        (QuantumRange-OpaqueOpacity));
      Da=QuantumScale*(reconstruct_image->matte != MagickFalse ?
        GetPixelAlpha(q) : (QuantumRange-OpaqueOpacity));
      if ((channel & RedChannel) != 0)
        {
          distance=QuantumScale*(Sa*GetPixelRed(p)-Da*GetPixelRed(q));
          channel_distortion[RedChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      if ((channel & GreenChannel) != 0)
        {
          distance=QuantumScale*(Sa*GetPixelGreen(p)-Da*GetPixelGreen(q));
          channel_distortion[GreenChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      if ((channel & BlueChannel) != 0)
        {
          distance=QuantumScale*(Sa*GetPixelBlue(p)-Da*GetPixelBlue(q));
          channel_distortion[BlueChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      if (((channel & OpacityChannel) != 0) && ((image->matte != MagickFalse) ||
          (reconstruct_image->matte != MagickFalse)))
        {
          distance=QuantumScale*((image->matte != MagickFalse ?
            GetPixelOpacity(p) : OpaqueOpacity)-
            (reconstruct_image->matte != MagickFalse ?
            GetPixelOpacity(q): OpaqueOpacity));
          channel_distortion[OpacityChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      if (((channel & IndexChannel) != 0) &&
          (image->colorspace == CMYKColorspace) &&
          (reconstruct_image->colorspace == CMYKColorspace))
        {
          distance=QuantumScale*(Sa*GetPixelIndex(indexes+x)-
            Da*GetPixelIndex(reconstruct_indexes+x));
          channel_distortion[BlackChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      p++;
      q++;
    }
#if defined(MAGICKCORE_OPENMP_SUPPORT)
    #pragma omp critical (MagickCore_GetFuzzDistortion)
#endif
    for (i=0; i <= (ssize_t) CompositeChannels; i++)
      distortion[i]+=channel_distortion[i];
  }
  reconstruct_view=DestroyCacheView(reconstruct_view);
  image_view=DestroyCacheView(image_view);
  for (i=0; i <= (ssize_t) CompositeChannels; i++)
    distortion[i]/=((double) columns*rows);
  distortion[CompositeChannels]/=(double) GetNumberChannels(image,channel);
  distortion[CompositeChannels]=sqrt(distortion[CompositeChannels]);
  return(status);
}

static MagickBooleanType GetMeanAbsoluteDistortion(const Image *image,
  const Image *reconstruct_image,const ChannelType channel,
  double *distortion,ExceptionInfo *exception)
{
  CacheView
    *image_view,
    *reconstruct_view;

  MagickBooleanType
    status;

  register ssize_t
    i;

  size_t
    columns,
    rows;

  ssize_t
    y;

  status=MagickTrue;
  rows=MagickMax(image->rows,reconstruct_image->rows);
  columns=MagickMax(image->columns,reconstruct_image->columns);
  image_view=AcquireVirtualCacheView(image,exception);
  reconstruct_view=AcquireVirtualCacheView(reconstruct_image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(status) \
    magick_number_threads(image,image,rows,1)
#endif
  for (y=0; y < (ssize_t) rows; y++)
  {
    double
      channel_distortion[CompositeChannels+1];

    register const IndexPacket
      *magick_restrict indexes,
      *magick_restrict reconstruct_indexes;

    register const PixelPacket
      *magick_restrict p,
      *magick_restrict q;

    register ssize_t
      i,
      x;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,columns,1,exception);
    q=GetCacheViewVirtualPixels(reconstruct_view,0,y,columns,1,exception);
    if ((p == (const PixelPacket *) NULL) || (q == (const PixelPacket *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    reconstruct_indexes=GetCacheViewVirtualIndexQueue(reconstruct_view);
    (void) memset(channel_distortion,0,sizeof(channel_distortion));
    for (x=0; x < (ssize_t) columns; x++)
    {
      MagickRealType
        distance,
        Da,
        Sa;

      Sa=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(p) :
        (QuantumRange-OpaqueOpacity));
      Da=QuantumScale*(reconstruct_image->matte != MagickFalse ?
        GetPixelAlpha(q) : (QuantumRange-OpaqueOpacity));
      if ((channel & RedChannel) != 0)
        {
          distance=QuantumScale*fabs(Sa*GetPixelRed(p)-Da*GetPixelRed(q));
          channel_distortion[RedChannel]+=distance;
          channel_distortion[CompositeChannels]+=distance;
        }
      if ((channel & GreenChannel) != 0)
        {
          distance=QuantumScale*fabs(Sa*GetPixelGreen(p)-Da*GetPixelGreen(q));
          channel_distortion[GreenChannel]+=distance;
          channel_distortion[CompositeChannels]+=distance;
        }
      if ((channel & BlueChannel) != 0)
        {
          distance=QuantumScale*fabs(Sa*GetPixelBlue(p)-Da*GetPixelBlue(q));
          channel_distortion[BlueChannel]+=distance;
          channel_distortion[CompositeChannels]+=distance;
        }
      if (((channel & OpacityChannel) != 0) &&
          (image->matte != MagickFalse))
        {
          distance=QuantumScale*fabs(GetPixelOpacity(p)-(double)
            GetPixelOpacity(q));
          channel_distortion[OpacityChannel]+=distance;
          channel_distortion[CompositeChannels]+=distance;
        }
      if (((channel & IndexChannel) != 0) &&
          (image->colorspace == CMYKColorspace))
        {
          distance=QuantumScale*fabs(Sa*GetPixelIndex(indexes+x)-Da*
            GetPixelIndex(reconstruct_indexes+x));
          channel_distortion[BlackChannel]+=distance;
          channel_distortion[CompositeChannels]+=distance;
        }
      p++;
      q++;
    }
#if defined(MAGICKCORE_OPENMP_SUPPORT)
    #pragma omp critical (MagickCore_GetMeanAbsoluteError)
#endif
    for (i=0; i <= (ssize_t) CompositeChannels; i++)
      distortion[i]+=channel_distortion[i];
  }
  reconstruct_view=DestroyCacheView(reconstruct_view);
  image_view=DestroyCacheView(image_view);
  for (i=0; i <= (ssize_t) CompositeChannels; i++)
    distortion[i]/=((double) columns*rows);
  distortion[CompositeChannels]/=(double) GetNumberChannels(image,channel);
  return(status);
}

static MagickBooleanType GetMeanErrorPerPixel(Image *image,
  const Image *reconstruct_image,const ChannelType channel,double *distortion,
  ExceptionInfo *exception)
{
  CacheView
    *image_view,
    *reconstruct_view;

  MagickBooleanType
    status;

  MagickRealType
    area,
    gamma,
    maximum_error,
    mean_error;

  size_t
    columns,
    rows;

  ssize_t
    y;

  status=MagickTrue;
  area=0.0;
  maximum_error=0.0;
  mean_error=0.0;
  rows=MagickMax(image->rows,reconstruct_image->rows);
  columns=MagickMax(image->columns,reconstruct_image->columns);
  image_view=AcquireVirtualCacheView(image,exception);
  reconstruct_view=AcquireVirtualCacheView(reconstruct_image,exception);
  for (y=0; y < (ssize_t) rows; y++)
  {
    register const IndexPacket
      *magick_restrict indexes,
      *magick_restrict reconstruct_indexes;

    register const PixelPacket
      *magick_restrict p,
      *magick_restrict q;

    register ssize_t
      x;

    p=GetCacheViewVirtualPixels(image_view,0,y,columns,1,exception);
    q=GetCacheViewVirtualPixels(reconstruct_view,0,y,columns,1,exception);
    if ((p == (const PixelPacket *) NULL) || (q == (const PixelPacket *) NULL))
      {
        status=MagickFalse;
        break;
      }
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    reconstruct_indexes=GetCacheViewVirtualIndexQueue(reconstruct_view);
    for (x=0; x < (ssize_t) columns; x++)
    {
      MagickRealType
        distance,
        Da,
        Sa;

      Sa=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(p) :
        (QuantumRange-OpaqueOpacity));
      Da=QuantumScale*(reconstruct_image->matte != MagickFalse ?
        GetPixelAlpha(q) : (QuantumRange-OpaqueOpacity));
      if ((channel & RedChannel) != 0)
        {
          distance=fabs(Sa*GetPixelRed(p)-Da*GetPixelRed(q));
          distortion[RedChannel]+=distance;
          distortion[CompositeChannels]+=distance;
          mean_error+=distance*distance;
          if (distance > maximum_error)
            maximum_error=distance;
          area++;
        }
      if ((channel & GreenChannel) != 0)
        {
          distance=fabs(Sa*GetPixelGreen(p)-Da*GetPixelGreen(q));
          distortion[GreenChannel]+=distance;
          distortion[CompositeChannels]+=distance;
          mean_error+=distance*distance;
          if (distance > maximum_error)
            maximum_error=distance;
          area++;
        }
      if ((channel & BlueChannel) != 0)
        {
          distance=fabs(Sa*GetPixelBlue(p)-Da*GetPixelBlue(q));
          distortion[BlueChannel]+=distance;
          distortion[CompositeChannels]+=distance;
          mean_error+=distance*distance;
          if (distance > maximum_error)
            maximum_error=distance;
          area++;
        }
      if (((channel & OpacityChannel) != 0) &&
          (image->matte != MagickFalse))
        {
          distance=fabs((double) GetPixelOpacity(p)-
            GetPixelOpacity(q));
          distortion[OpacityChannel]+=distance;
          distortion[CompositeChannels]+=distance;
          mean_error+=distance*distance;
          if (distance > maximum_error)
            maximum_error=distance;
          area++;
        }
      if (((channel & IndexChannel) != 0) &&
          (image->colorspace == CMYKColorspace) &&
          (reconstruct_image->colorspace == CMYKColorspace))
        {
          distance=fabs(Sa*GetPixelIndex(indexes+x)-Da*
            GetPixelIndex(reconstruct_indexes+x));
          distortion[BlackChannel]+=distance;
          distortion[CompositeChannels]+=distance;
          mean_error+=distance*distance;
          if (distance > maximum_error)
            maximum_error=distance;
          area++;
        }
      p++;
      q++;
    }
  }
  reconstruct_view=DestroyCacheView(reconstruct_view);
  image_view=DestroyCacheView(image_view);
  gamma=PerceptibleReciprocal(area);
  image->error.mean_error_per_pixel=gamma*distortion[CompositeChannels];
  image->error.normalized_mean_error=gamma*QuantumScale*QuantumScale*mean_error;
  image->error.normalized_maximum_error=QuantumScale*maximum_error;
  return(status);
}

static MagickBooleanType GetMeanSquaredDistortion(const Image *image,
  const Image *reconstruct_image,const ChannelType channel,
  double *distortion,ExceptionInfo *exception)
{
  CacheView
    *image_view,
    *reconstruct_view;

  MagickBooleanType
    status;

  register ssize_t
    i;

  size_t
    columns,
    rows;

  ssize_t
    y;

  status=MagickTrue;
  rows=MagickMax(image->rows,reconstruct_image->rows);
  columns=MagickMax(image->columns,reconstruct_image->columns);
  image_view=AcquireVirtualCacheView(image,exception);
  reconstruct_view=AcquireVirtualCacheView(reconstruct_image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(status) \
    magick_number_threads(image,image,rows,1)
#endif
  for (y=0; y < (ssize_t) rows; y++)
  {
    double
      channel_distortion[CompositeChannels+1];

    register const IndexPacket
      *magick_restrict indexes,
      *magick_restrict reconstruct_indexes;

    register const PixelPacket
      *magick_restrict p,
      *magick_restrict q;

    register ssize_t
      i,
      x;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,columns,1,exception);
    q=GetCacheViewVirtualPixels(reconstruct_view,0,y,columns,1,exception);
    if ((p == (const PixelPacket *) NULL) || (q == (const PixelPacket *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    reconstruct_indexes=GetCacheViewVirtualIndexQueue(reconstruct_view);
    (void) memset(channel_distortion,0,sizeof(channel_distortion));
    for (x=0; x < (ssize_t) columns; x++)
    {
      MagickRealType
        distance,
        Da,
        Sa;

      Sa=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(p) :
        (QuantumRange-OpaqueOpacity));
      Da=QuantumScale*(reconstruct_image->matte != MagickFalse ?
        GetPixelAlpha(q) : (QuantumRange-OpaqueOpacity));
      if ((channel & RedChannel) != 0)
        {
          distance=QuantumScale*(Sa*GetPixelRed(p)-Da*GetPixelRed(q));
          channel_distortion[RedChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      if ((channel & GreenChannel) != 0)
        {
          distance=QuantumScale*(Sa*GetPixelGreen(p)-Da*GetPixelGreen(q));
          channel_distortion[GreenChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      if ((channel & BlueChannel) != 0)
        {
          distance=QuantumScale*(Sa*GetPixelBlue(p)-Da*GetPixelBlue(q));
          channel_distortion[BlueChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      if (((channel & OpacityChannel) != 0) &&
          (image->matte != MagickFalse))
        {
          distance=QuantumScale*(GetPixelOpacity(p)-(MagickRealType)
            GetPixelOpacity(q));
          channel_distortion[OpacityChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      if (((channel & IndexChannel) != 0) &&
          (image->colorspace == CMYKColorspace) &&
          (reconstruct_image->colorspace == CMYKColorspace))
        {
          distance=QuantumScale*(Sa*GetPixelIndex(indexes+x)-Da*
            GetPixelIndex(reconstruct_indexes+x));
          channel_distortion[BlackChannel]+=distance*distance;
          channel_distortion[CompositeChannels]+=distance*distance;
        }
      p++;
      q++;
    }
#if defined(MAGICKCORE_OPENMP_SUPPORT)
    #pragma omp critical (MagickCore_GetMeanSquaredError)
#endif
    for (i=0; i <= (ssize_t) CompositeChannels; i++)
      distortion[i]+=channel_distortion[i];
  }
  reconstruct_view=DestroyCacheView(reconstruct_view);
  image_view=DestroyCacheView(image_view);
  for (i=0; i <= (ssize_t) CompositeChannels; i++)
    distortion[i]/=((double) columns*rows);
  distortion[CompositeChannels]/=(double) GetNumberChannels(image,channel);
  return(status);
}

static MagickBooleanType GetNormalizedCrossCorrelationDistortion(
  const Image *image,const Image *reconstruct_image,const ChannelType channel,
  double *distortion,ExceptionInfo *exception)
{
#define SimilarityImageTag  "Similarity/Image"

  CacheView
    *image_view,
    *reconstruct_view;

  ChannelStatistics
    *image_statistics,
    *reconstruct_statistics;

  MagickBooleanType
    status;

  MagickOffsetType
    progress;

  MagickRealType
    area;

  register ssize_t
    i;

  size_t
    columns,
    rows;

  ssize_t
    y;

  /*
    Normalize to account for variation due to lighting and exposure condition.
  */
  image_statistics=GetImageChannelStatistics(image,exception);
  reconstruct_statistics=GetImageChannelStatistics(reconstruct_image,exception);
  if ((image_statistics == (ChannelStatistics *) NULL) ||
      (reconstruct_statistics == (ChannelStatistics *) NULL))
    {
      if (image_statistics != (ChannelStatistics *) NULL)
        image_statistics=(ChannelStatistics *) RelinquishMagickMemory(
          image_statistics);
      if (reconstruct_statistics != (ChannelStatistics *) NULL)
        reconstruct_statistics=(ChannelStatistics *) RelinquishMagickMemory(
          reconstruct_statistics);
      return(MagickFalse);
    }
  status=MagickTrue;
  progress=0;
  for (i=0; i <= (ssize_t) CompositeChannels; i++)
    distortion[i]=0.0;
  rows=MagickMax(image->rows,reconstruct_image->rows);
  columns=MagickMax(image->columns,reconstruct_image->columns);
  area=1.0/((MagickRealType) columns*rows);
  image_view=AcquireVirtualCacheView(image,exception);
  reconstruct_view=AcquireVirtualCacheView(reconstruct_image,exception);
  for (y=0; y < (ssize_t) rows; y++)
  {
    register const IndexPacket
      *magick_restrict indexes,
      *magick_restrict reconstruct_indexes;

    register const PixelPacket
      *magick_restrict p,
      *magick_restrict q;

    register ssize_t
      x;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,columns,1,exception);
    q=GetCacheViewVirtualPixels(reconstruct_view,0,y,columns,1,exception);
    if ((p == (const PixelPacket *) NULL) || (q == (const PixelPacket *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    reconstruct_indexes=GetCacheViewVirtualIndexQueue(reconstruct_view);
    for (x=0; x < (ssize_t) columns; x++)
    {
      MagickRealType
        Da,
        Sa;

      Sa=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(p) :
        (QuantumRange-OpaqueOpacity));
      Da=QuantumScale*(reconstruct_image->matte != MagickFalse ?
        GetPixelAlpha(q) : (QuantumRange-OpaqueOpacity));
      if ((channel & RedChannel) != 0)
        distortion[RedChannel]+=area*QuantumScale*(Sa*GetPixelRed(p)-
          image_statistics[RedChannel].mean)*(Da*GetPixelRed(q)-
          reconstruct_statistics[RedChannel].mean);
      if ((channel & GreenChannel) != 0)
        distortion[GreenChannel]+=area*QuantumScale*(Sa*GetPixelGreen(p)-
          image_statistics[GreenChannel].mean)*(Da*GetPixelGreen(q)-
          reconstruct_statistics[GreenChannel].mean);
      if ((channel & BlueChannel) != 0)
        distortion[BlueChannel]+=area*QuantumScale*(Sa*GetPixelBlue(p)-
          image_statistics[BlueChannel].mean)*(Da*GetPixelBlue(q)-
          reconstruct_statistics[BlueChannel].mean);
      if (((channel & OpacityChannel) != 0) && (image->matte != MagickFalse))
        distortion[OpacityChannel]+=area*QuantumScale*(
          GetPixelOpacity(p)-image_statistics[OpacityChannel].mean)*
          (GetPixelOpacity(q)-reconstruct_statistics[OpacityChannel].mean);
      if (((channel & IndexChannel) != 0) &&
          (image->colorspace == CMYKColorspace) &&
          (reconstruct_image->colorspace == CMYKColorspace))
        distortion[BlackChannel]+=area*QuantumScale*(Sa*
          GetPixelIndex(indexes+x)-image_statistics[BlackChannel].mean)*(Da*
          GetPixelIndex(reconstruct_indexes+x)-
          reconstruct_statistics[BlackChannel].mean);
      p++;
      q++;
    }
    if (image->progress_monitor != (MagickProgressMonitor) NULL)
      {
        MagickBooleanType
          proceed;

        proceed=SetImageProgress(image,SimilarityImageTag,progress++,rows);
        if (proceed == MagickFalse)
          status=MagickFalse;
      }
  }
  reconstruct_view=DestroyCacheView(reconstruct_view);
  image_view=DestroyCacheView(image_view);
  /*
    Divide by the standard deviation.
  */
  for (i=0; i < (ssize_t) CompositeChannels; i++)
  {
    double
      gamma;

    gamma=image_statistics[i].standard_deviation*
      reconstruct_statistics[i].standard_deviation;
    gamma=PerceptibleReciprocal(gamma);
    distortion[i]=QuantumRange*gamma*distortion[i];
  }
  distortion[CompositeChannels]=0.0;
  if ((channel & RedChannel) != 0)
    distortion[CompositeChannels]+=distortion[RedChannel]*
      distortion[RedChannel];
  if ((channel & GreenChannel) != 0)
    distortion[CompositeChannels]+=distortion[GreenChannel]*
      distortion[GreenChannel];
  if ((channel & BlueChannel) != 0)
    distortion[CompositeChannels]+=distortion[BlueChannel]*
      distortion[BlueChannel];
  if (((channel & OpacityChannel) != 0) && (image->matte != MagickFalse))
    distortion[CompositeChannels]+=distortion[OpacityChannel]*
      distortion[OpacityChannel];
  if (((channel & IndexChannel) != 0) && (image->colorspace == CMYKColorspace))
    distortion[CompositeChannels]+=distortion[BlackChannel]*
      distortion[BlackChannel];
  distortion[CompositeChannels]=sqrt(distortion[CompositeChannels]/
    GetNumberChannels(image,channel));
  /*
    Free resources.
  */
  reconstruct_statistics=(ChannelStatistics *) RelinquishMagickMemory(
    reconstruct_statistics);
  image_statistics=(ChannelStatistics *) RelinquishMagickMemory(
    image_statistics);
  return(status);
}

static MagickBooleanType GetPeakAbsoluteDistortion(const Image *image,
  const Image *reconstruct_image,const ChannelType channel,
  double *distortion,ExceptionInfo *exception)
{
  CacheView
    *image_view,
    *reconstruct_view;

  MagickBooleanType
    status;

  size_t
    columns,
    rows;

  ssize_t
    y;

  status=MagickTrue;
  rows=MagickMax(image->rows,reconstruct_image->rows);
  columns=MagickMax(image->columns,reconstruct_image->columns);
  image_view=AcquireVirtualCacheView(image,exception);
  reconstruct_view=AcquireVirtualCacheView(reconstruct_image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(status) \
    magick_number_threads(image,image,rows,1)
#endif
  for (y=0; y < (ssize_t) rows; y++)
  {
    double
      channel_distortion[CompositeChannels+1];

    register const IndexPacket
      *magick_restrict indexes,
      *magick_restrict reconstruct_indexes;

    register const PixelPacket
      *magick_restrict p,
      *magick_restrict q;

    register ssize_t
      i,
      x;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(image_view,0,y,columns,1,exception);
    q=GetCacheViewVirtualPixels(reconstruct_view,0,y,columns,1,exception);
    if ((p == (const PixelPacket *) NULL) || (q == (const PixelPacket *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    reconstruct_indexes=GetCacheViewVirtualIndexQueue(reconstruct_view);
    (void) memset(channel_distortion,0,sizeof(channel_distortion));
    for (x=0; x < (ssize_t) columns; x++)
    {
      MagickRealType
        distance,
        Da,
        Sa;

      Sa=QuantumScale*(image->matte != MagickFalse ? GetPixelAlpha(p) :
        (QuantumRange-OpaqueOpacity));
      Da=QuantumScale*(reconstruct_image->matte != MagickFalse ?
        GetPixelAlpha(q) : (QuantumRange-OpaqueOpacity));
      if ((channel & RedChannel) != 0)
        {
          distance=QuantumScale*fabs(Sa*GetPixelRed(p)-Da*GetPixelRed(q));
          if (distance > channel_distortion[RedChannel])
            channel_distortion[RedChannel]=distance;
          if (distance > channel_distortion[CompositeChannels])
            channel_distortion[CompositeChannels]=distance;
        }
      if ((channel & GreenChannel) != 0)
        {
          distance=QuantumScale*fabs(Sa*GetPixelGreen(p)-Da*GetPixelGreen(q));
          if (distance > channel_distortion[GreenChannel])
            channel_distortion[GreenChannel]=distance;
          if (distance > channel_distortion[CompositeChannels])
            channel_distortion[CompositeChannels]=distance;
        }
      if ((channel & BlueChannel) != 0)
        {
          distance=QuantumScale*fabs(Sa*GetPixelBlue(p)-Da*GetPixelBlue(q));
          if (distance > channel_distortion[BlueChannel])
            channel_distortion[BlueChannel]=distance;
          if (distance > channel_distortion[CompositeChannels])
            channel_distortion[CompositeChannels]=distance;
        }
      if (((channel & OpacityChannel) != 0) &&
          (image->matte != MagickFalse))
        {
          distance=QuantumScale*fabs(GetPixelOpacity(p)-(double)
            GetPixelOpacity(q));
          if (distance > channel_distortion[OpacityChannel])
            channel_distortion[OpacityChannel]=distance;
          if (distance > channel_distortion[CompositeChannels])
            channel_distortion[CompositeChannels]=distance;
        }
      if (((channel & IndexChannel) != 0) &&
          (image->colorspace == CMYKColorspace) &&
          (reconstruct_image->colorspace == CMYKColorspace))
        {
          distance=QuantumScale*fabs(Sa*GetPixelIndex(indexes+x)-Da*
            GetPixelIndex(reconstruct_indexes+x));
          if (distance > channel_distortion[BlackChannel])
            channel_distortion[BlackChannel]=distance;
          if (distance > channel_distortion[CompositeChannels])
            channel_distortion[CompositeChannels]=distance;
        }
      p++;
      q++;
    }
#if defined(MAGICKCORE_OPENMP_SUPPORT)
    #pragma omp critical (MagickCore_GetPeakAbsoluteError)
#endif
    for (i=0; i <= (ssize_t) CompositeChannels; i++)
      if (channel_distortion[i] > distortion[i])
        distortion[i]=channel_distortion[i];
  }
  reconstruct_view=DestroyCacheView(reconstruct_view);
  image_view=DestroyCacheView(image_view);
  return(status);
}

static inline double MagickLog10(const double x)
{
#define Log10Epsilon  (1.0e-11)

 if (fabs(x) < Log10Epsilon)
   return(log10(Log10Epsilon));
 return(log10(fabs(x)));
}

static MagickBooleanType GetPeakSignalToNoiseRatio(const Image *image,
  const Image *reconstruct_image,const ChannelType channel,
  double *distortion,ExceptionInfo *exception)
{
  MagickBooleanType
    status;

  status=GetMeanSquaredDistortion(image,reconstruct_image,channel,distortion,
    exception);
  if ((channel & RedChannel) != 0)
    {
      if (fabs(distortion[RedChannel]) < MagickEpsilon)
        distortion[RedChannel]=INFINITY;
      else
        distortion[RedChannel]=10.0*MagickLog10(1.0)-10.0*
          MagickLog10(distortion[RedChannel]);
    }
  if ((channel & GreenChannel) != 0)
    {
      if (fabs(distortion[GreenChannel]) < MagickEpsilon)
        distortion[GreenChannel]=INFINITY;
      else
        distortion[GreenChannel]=10.0*MagickLog10(1.0)-10.0*
          MagickLog10(distortion[GreenChannel]);
    }
  if ((channel & BlueChannel) != 0)
    {
      if (fabs(distortion[BlueChannel]) < MagickEpsilon)
        distortion[BlueChannel]=INFINITY;
      else
        distortion[BlueChannel]=10.0*MagickLog10(1.0)-10.0*
          MagickLog10(distortion[BlueChannel]);
    }
  if (((channel & OpacityChannel) != 0) && (image->matte != MagickFalse))
    {
      if (fabs(distortion[OpacityChannel]) < MagickEpsilon)
        distortion[OpacityChannel]=INFINITY;
      else
        distortion[OpacityChannel]=10.0*MagickLog10(1.0)-10.0*
          MagickLog10(distortion[OpacityChannel]);
    }
  if (((channel & IndexChannel) != 0) && (image->colorspace == CMYKColorspace))
    {
      if (fabs(distortion[BlackChannel]) < MagickEpsilon)
        distortion[BlackChannel]=INFINITY;
      else
        distortion[BlackChannel]=10.0*MagickLog10(1.0)-10.0*
          MagickLog10(distortion[BlackChannel]);
    }
  if (fabs(distortion[CompositeChannels]) < MagickEpsilon)
    distortion[CompositeChannels]=INFINITY;
  else
    distortion[CompositeChannels]=10.0*MagickLog10(1.0)-10.0*
      MagickLog10(distortion[CompositeChannels]);
  return(status);
}

static MagickBooleanType GetPerceptualHashDistortion(const Image *image,
  const Image *reconstruct_image,const ChannelType channel,double *distortion,
  ExceptionInfo *exception)
{
  ChannelPerceptualHash
    *image_phash,
    *reconstruct_phash;

  double
    difference;

  register ssize_t
    i;

  /*
    Compute perceptual hash in the sRGB colorspace.
  */
  image_phash=GetImageChannelPerceptualHash(image,exception);
  if (image_phash == (ChannelPerceptualHash *) NULL)
    return(MagickFalse);
  reconstruct_phash=GetImageChannelPerceptualHash(reconstruct_image,exception);
  if (reconstruct_phash == (ChannelPerceptualHash *) NULL)
    {
      image_phash=(ChannelPerceptualHash *) RelinquishMagickMemory(image_phash);
      return(MagickFalse);
    }
  for (i=0; i < MaximumNumberOfImageMoments; i++)
  {
    /*
      Compute sum of moment differences squared.
    */
    if ((channel & RedChannel) != 0)
      {
        difference=reconstruct_phash[RedChannel].P[i]-
          image_phash[RedChannel].P[i];
        distortion[RedChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
    if ((channel & GreenChannel) != 0)
      {
        difference=reconstruct_phash[GreenChannel].P[i]-
          image_phash[GreenChannel].P[i];
        distortion[GreenChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
    if ((channel & BlueChannel) != 0)
      {
        difference=reconstruct_phash[BlueChannel].P[i]-
          image_phash[BlueChannel].P[i];
        distortion[BlueChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
    if (((channel & OpacityChannel) != 0) && (image->matte != MagickFalse) &&
        (reconstruct_image->matte != MagickFalse))
      {
        difference=reconstruct_phash[OpacityChannel].P[i]-
          image_phash[OpacityChannel].P[i];
        distortion[OpacityChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
    if (((channel & IndexChannel) != 0) &&
        (image->colorspace == CMYKColorspace) &&
        (reconstruct_image->colorspace == CMYKColorspace))
      {
        difference=reconstruct_phash[IndexChannel].P[i]-
          image_phash[IndexChannel].P[i];
        distortion[IndexChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
  }
  /*
    Compute perceptual hash in the HCLP colorspace.
  */
  for (i=0; i < MaximumNumberOfImageMoments; i++)
  {
    /*
      Compute sum of moment differences squared.
    */
    if ((channel & RedChannel) != 0)
      {
        difference=reconstruct_phash[RedChannel].Q[i]-
          image_phash[RedChannel].Q[i];
        distortion[RedChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
    if ((channel & GreenChannel) != 0)
      {
        difference=reconstruct_phash[GreenChannel].Q[i]-
          image_phash[GreenChannel].Q[i];
        distortion[GreenChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
    if ((channel & BlueChannel) != 0)
      {
        difference=reconstruct_phash[BlueChannel].Q[i]-
          image_phash[BlueChannel].Q[i];
        distortion[BlueChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
    if (((channel & OpacityChannel) != 0) && (image->matte != MagickFalse) &&
        (reconstruct_image->matte != MagickFalse))
      {
        difference=reconstruct_phash[OpacityChannel].Q[i]-
          image_phash[OpacityChannel].Q[i];
        distortion[OpacityChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
    if (((channel & IndexChannel) != 0) &&
        (image->colorspace == CMYKColorspace) &&
        (reconstruct_image->colorspace == CMYKColorspace))
      {
        difference=reconstruct_phash[IndexChannel].Q[i]-
          image_phash[IndexChannel].Q[i];
        distortion[IndexChannel]+=difference*difference;
        distortion[CompositeChannels]+=difference*difference;
      }
  }
  /*
    Free resources.
  */
  reconstruct_phash=(ChannelPerceptualHash *) RelinquishMagickMemory(
    reconstruct_phash);
  image_phash=(ChannelPerceptualHash *) RelinquishMagickMemory(image_phash);
  return(MagickTrue);
}

static MagickBooleanType GetRootMeanSquaredDistortion(const Image *image,
  const Image *reconstruct_image,const ChannelType channel,double *distortion,
  ExceptionInfo *exception)
{
  MagickBooleanType
    status;

  status=GetMeanSquaredDistortion(image,reconstruct_image,channel,distortion,
    exception);
  if ((channel & RedChannel) != 0)
    distortion[RedChannel]=sqrt(distortion[RedChannel]);
  if ((channel & GreenChannel) != 0)
    distortion[GreenChannel]=sqrt(distortion[GreenChannel]);
  if ((channel & BlueChannel) != 0)
    distortion[BlueChannel]=sqrt(distortion[BlueChannel]);
  if (((channel & OpacityChannel) != 0) &&
      (image->matte != MagickFalse))
    distortion[OpacityChannel]=sqrt(distortion[OpacityChannel]);
  if (((channel & IndexChannel) != 0) &&
      (image->colorspace == CMYKColorspace))
    distortion[BlackChannel]=sqrt(distortion[BlackChannel]);
  distortion[CompositeChannels]=sqrt(distortion[CompositeChannels]);
  return(status);
}

MagickExport MagickBooleanType GetImageChannelDistortion(Image *image,
  const Image *reconstruct_image,const ChannelType channel,
  const MetricType metric,double *distortion,ExceptionInfo *exception)
{
  double
    *channel_distortion;

  MagickBooleanType
    status;

  size_t
    length;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(reconstruct_image != (const Image *) NULL);
  assert(reconstruct_image->signature == MagickCoreSignature);
  assert(distortion != (double *) NULL);
  *distortion=0.0;
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if (metric != PerceptualHashErrorMetric)
    if (ValidateImageMorphology(image,reconstruct_image) == MagickFalse)
      ThrowBinaryException(ImageError,"ImageMorphologyDiffers",image->filename);
  /*
    Get image distortion.
  */
  length=CompositeChannels+1UL;
  channel_distortion=(double *) AcquireQuantumMemory(length,
    sizeof(*channel_distortion));
  if (channel_distortion == (double *) NULL)
    ThrowFatalException(ResourceLimitFatalError,"MemoryAllocationFailed");
  (void) memset(channel_distortion,0,length*sizeof(*channel_distortion));
  switch (metric)
  {
    case AbsoluteErrorMetric:
    {
      status=GetAbsoluteDistortion(image,reconstruct_image,channel,
        channel_distortion,exception);
      break;
    }
    case FuzzErrorMetric:
    {
      status=GetFuzzDistortion(image,reconstruct_image,channel,
        channel_distortion,exception);
      break;
    }
    case MeanAbsoluteErrorMetric:
    {
      status=GetMeanAbsoluteDistortion(image,reconstruct_image,channel,
        channel_distortion,exception);
      break;
    }
    case MeanErrorPerPixelMetric:
    {
      status=GetMeanErrorPerPixel(image,reconstruct_image,channel,
        channel_distortion,exception);
      break;
    }
    case MeanSquaredErrorMetric:
    {
      status=GetMeanSquaredDistortion(image,reconstruct_image,channel,
        channel_distortion,exception);
      break;
    }
    case NormalizedCrossCorrelationErrorMetric:
    default:
    {
      status=GetNormalizedCrossCorrelationDistortion(image,reconstruct_image,
        channel,channel_distortion,exception);
      break;
    }
    case PeakAbsoluteErrorMetric:
    {
      status=GetPeakAbsoluteDistortion(image,reconstruct_image,channel,
        channel_distortion,exception);
      break;
    }
    case PeakSignalToNoiseRatioMetric:
    {
      status=GetPeakSignalToNoiseRatio(image,reconstruct_image,channel,
        channel_distortion,exception);
      break;
    }
    case PerceptualHashErrorMetric:
    {
      status=GetPerceptualHashDistortion(image,reconstruct_image,channel,
        channel_distortion,exception);
      break;
    }
    case RootMeanSquaredErrorMetric:
    {
      status=GetRootMeanSquaredDistortion(image,reconstruct_image,channel,
        channel_distortion,exception);
      break;
    }
  }
  *distortion=channel_distortion[CompositeChannels];
  channel_distortion=(double *) RelinquishMagickMemory(channel_distortion);
  (void) FormatImageProperty(image,"distortion","%.*g",GetMagickPrecision(),
    *distortion);
  return(status);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   G e t I m a g e C h a n n e l D i s t o r t i o n s                       %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  GetImageChannelDistortions() compares the image channels of an image to a
%  reconstructed image and returns the specified distortion metric for each
%  channel.
%
%  The format of the GetImageChannelDistortions method is:
%
%      double *GetImageChannelDistortions(const Image *image,
%        const Image *reconstruct_image,const MetricType metric,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o reconstruct_image: the reconstruct image.
%
%    o metric: the metric.
%
%    o exception: return any errors or warnings in this structure.
%
*/
MagickExport double *GetImageChannelDistortions(Image *image,
  const Image *reconstruct_image,const MetricType metric,
  ExceptionInfo *exception)
{
  double
    *channel_distortion;

  MagickBooleanType
    status;

  size_t
    length;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(reconstruct_image != (const Image *) NULL);
  assert(reconstruct_image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if (metric != PerceptualHashErrorMetric)
    if (ValidateImageMorphology(image,reconstruct_image) == MagickFalse)
      {
        (void) ThrowMagickException(&image->exception,GetMagickModule(),
          ImageError,"ImageMorphologyDiffers","`%s'",image->filename);
        return((double *) NULL);
      }
  /*
    Get image distortion.
  */
  length=CompositeChannels+1UL;
  channel_distortion=(double *) AcquireQuantumMemory(length,
    sizeof(*channel_distortion));
  if (channel_distortion == (double *) NULL)
    ThrowFatalException(ResourceLimitFatalError,"MemoryAllocationFailed");
  (void) memset(channel_distortion,0,length*
    sizeof(*channel_distortion));
  status=MagickTrue;
  switch (metric)
  {
    case AbsoluteErrorMetric:
    {
      status=GetAbsoluteDistortion(image,reconstruct_image,CompositeChannels,
        channel_distortion,exception);
      break;
    }
    case FuzzErrorMetric:
    {
      status=GetFuzzDistortion(image,reconstruct_image,CompositeChannels,
        channel_distortion,exception);
      break;
    }
    case MeanAbsoluteErrorMetric:
    {
      status=GetMeanAbsoluteDistortion(image,reconstruct_image,
        CompositeChannels,channel_distortion,exception);
      break;
    }
    case MeanErrorPerPixelMetric:
    {
      status=GetMeanErrorPerPixel(image,reconstruct_image,CompositeChannels,
        channel_distortion,exception);
      break;
    }
    case MeanSquaredErrorMetric:
    {
      status=GetMeanSquaredDistortion(image,reconstruct_image,CompositeChannels,
        channel_distortion,exception);
      break;
    }
    case NormalizedCrossCorrelationErrorMetric:
    default:
    {
      status=GetNormalizedCrossCorrelationDistortion(image,reconstruct_image,
        CompositeChannels,channel_distortion,exception);
      break;
    }
    case PeakAbsoluteErrorMetric:
    {
      status=GetPeakAbsoluteDistortion(image,reconstruct_image,
        CompositeChannels,channel_distortion,exception);
      break;
    }
    case PeakSignalToNoiseRatioMetric:
    {
      status=GetPeakSignalToNoiseRatio(image,reconstruct_image,
        CompositeChannels,channel_distortion,exception);
      break;
    }
    case PerceptualHashErrorMetric:
    {
      status=GetPerceptualHashDistortion(image,reconstruct_image,
        CompositeChannels,channel_distortion,exception);
      break;
    }
    case RootMeanSquaredErrorMetric:
    {
      status=GetRootMeanSquaredDistortion(image,reconstruct_image,
        CompositeChannels,channel_distortion,exception);
      break;
    }
  }
  if (status == MagickFalse)
    {
      channel_distortion=(double *) RelinquishMagickMemory(channel_distortion);
      return((double *) NULL);
    }
  return(channel_distortion);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%  I s I m a g e s E q u a l                                                  %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IsImagesEqual() measures the difference between colors at each pixel
%  location of two images.  A value other than 0 means the colors match
%  exactly.  Otherwise an error measure is computed by summing over all
%  pixels in an image the distance squared in RGB space between each image
%  pixel and its corresponding pixel in the reconstruct image.  The error
%  measure is assigned to these image members:
%
%    o mean_error_per_pixel:  The mean error for any single pixel in
%      the image.
%
%    o normalized_mean_error:  The normalized mean quantization error for
%      any single pixel in the image.  This distance measure is normalized to
%      a range between 0 and 1.  It is independent of the range of red, green,
%      and blue values in the image.
%
%    o normalized_maximum_error:  The normalized maximum quantization
%      error for any single pixel in the image.  This distance measure is
%      normalized to a range between 0 and 1.  It is independent of the range
%      of red, green, and blue values in your image.
%
%  A small normalized mean square error, accessed as
%  image->normalized_mean_error, suggests the images are very similar in
%  spatial layout and color.
%
%  The format of the IsImagesEqual method is:
%
%      MagickBooleanType IsImagesEqual(Image *image,
%        const Image *reconstruct_image)
%
%  A description of each parameter follows.
%
%    o image: the image.
%
%    o reconstruct_image: the reconstruct image.
%
*/
MagickExport MagickBooleanType IsImagesEqual(Image *image,
  const Image *reconstruct_image)
{
  CacheView
    *image_view,
    *reconstruct_view;

  ExceptionInfo
    *exception;

  MagickBooleanType
    status;

  MagickRealType
    area,
    gamma,
    maximum_error,
    mean_error,
    mean_error_per_pixel;

  size_t
    columns,
    rows;

  ssize_t
    y;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  assert(reconstruct_image != (const Image *) NULL);
  assert(reconstruct_image->signature == MagickCoreSignature);
  exception=(&image->exception);
  if (ValidateImageMorphology(image,reconstruct_image) == MagickFalse)
    ThrowBinaryException(ImageError,"ImageMorphologyDiffers",image->filename);
  area=0.0;
  maximum_error=0.0;
  mean_error_per_pixel=0.0;
  mean_error=0.0;
  rows=MagickMax(image->rows,reconstruct_image->rows);
  columns=MagickMax(image->columns,reconstruct_image->columns);
  image_view=AcquireVirtualCacheView(image,exception);
  reconstruct_view=AcquireVirtualCacheView(reconstruct_image,exception);
  for (y=0; y < (ssize_t) rows; y++)
  {
    register const IndexPacket
      *magick_restrict indexes,
      *magick_restrict reconstruct_indexes;

    register const PixelPacket
      *magick_restrict p,
      *magick_restrict q;

    register ssize_t
      x;

    p=GetCacheViewVirtualPixels(image_view,0,y,columns,1,exception);
    q=GetCacheViewVirtualPixels(reconstruct_view,0,y,columns,1,exception);
    if ((p == (const PixelPacket *) NULL) || (q == (const PixelPacket *) NULL))
      break;
    indexes=GetCacheViewVirtualIndexQueue(image_view);
    reconstruct_indexes=GetCacheViewVirtualIndexQueue(reconstruct_view);
    for (x=0; x < (ssize_t) columns; x++)
    {
      MagickRealType
        distance;

      distance=fabs(GetPixelRed(p)-(double) GetPixelRed(q));
      mean_error_per_pixel+=distance;
      mean_error+=distance*distance;
      if (distance > maximum_error)
        maximum_error=distance;
      area++;
      distance=fabs(GetPixelGreen(p)-(double) GetPixelGreen(q));
      mean_error_per_pixel+=distance;
      mean_error+=distance*distance;
      if (distance > maximum_error)
        maximum_error=distance;
      area++;
      distance=fabs(GetPixelBlue(p)-(double) GetPixelBlue(q));
      mean_error_per_pixel+=distance;
      mean_error+=distance*distance;
      if (distance > maximum_error)
        maximum_error=distance;
      area++;
      if (image->matte != MagickFalse)
        {
          distance=fabs(GetPixelOpacity(p)-(double) GetPixelOpacity(q));
          mean_error_per_pixel+=distance;
          mean_error+=distance*distance;
          if (distance > maximum_error)
            maximum_error=distance;
          area++;
        }
      if ((image->colorspace == CMYKColorspace) &&
          (reconstruct_image->colorspace == CMYKColorspace))
        {
          distance=fabs(GetPixelIndex(indexes+x)-(double)
            GetPixelIndex(reconstruct_indexes+x));
          mean_error_per_pixel+=distance;
          mean_error+=distance*distance;
          if (distance > maximum_error)
            maximum_error=distance;
          area++;
        }
      p++;
      q++;
    }
  }
  reconstruct_view=DestroyCacheView(reconstruct_view);
  image_view=DestroyCacheView(image_view);
  gamma=PerceptibleReciprocal(area);
  image->error.mean_error_per_pixel=gamma*mean_error_per_pixel;
  image->error.normalized_mean_error=gamma*QuantumScale*QuantumScale*mean_error;
  image->error.normalized_maximum_error=QuantumScale*maximum_error;
  status=image->error.mean_error_per_pixel == 0.0 ? MagickTrue : MagickFalse;
  return(status);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   S i m i l a r i t y I m a g e                                             %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  SimilarityImage() compares the reference image of the image and returns the
%  best match offset.  In addition, it returns a similarity image such that an
%  exact match location is completely white and if none of the pixels match,
%  black, otherwise some gray level in-between.
%
%  The format of the SimilarityImageImage method is:
%
%      Image *SimilarityImage(const Image *image,const Image *reference,
%        RectangleInfo *offset,double *similarity,ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o reference: find an area of the image that closely resembles this image.
%
%    o the best match offset of the reference image within the image.
%
%    o similarity: the computed similarity between the images.
%
%    o exception: return any errors or warnings in this structure.
%
*/

static double GetSimilarityMetric(const Image *image,const Image *reference,
  const MetricType metric,const ssize_t x_offset,const ssize_t y_offset,
  ExceptionInfo *exception)
{
  double
    distortion;

  Image
    *similarity_image;

  MagickBooleanType
    status;

  RectangleInfo
    geometry;

  SetGeometry(reference,&geometry);
  geometry.x=x_offset;
  geometry.y=y_offset;
  similarity_image=CropImage(image,&geometry,exception);
  if (similarity_image == (Image *) NULL)
    return(0.0);
  distortion=0.0;
  status=GetImageDistortion(similarity_image,reference,metric,&distortion,
    exception);
  (void) status;
  similarity_image=DestroyImage(similarity_image);
  return(distortion);
}

MagickExport Image *SimilarityImage(Image *image,const Image *reference,
  RectangleInfo *offset,double *similarity_metric,ExceptionInfo *exception)
{
  Image
    *similarity_image;

  similarity_image=SimilarityMetricImage(image,reference,
    RootMeanSquaredErrorMetric,offset,similarity_metric,exception);
  return(similarity_image);
}

MagickExport Image *SimilarityMetricImage(Image *image,const Image *reference,
  const MetricType metric,RectangleInfo *offset,double *similarity_metric,
  ExceptionInfo *exception)
{
#define SimilarityImageTag  "Similarity/Image"

  CacheView
    *similarity_view;

  const char
    *artifact;

  double
    similarity_threshold;

  Image
    *similarity_image;

  MagickBooleanType
    status;

  MagickOffsetType
    progress;

  ssize_t
    y;

  assert(image != (const Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  assert(offset != (RectangleInfo *) NULL);
  SetGeometry(reference,offset);
  *similarity_metric=MagickMaximumValue;
  if (ValidateImageMorphology(image,reference) == MagickFalse)
    ThrowImageException(ImageError,"ImageMorphologyDiffers");
  similarity_image=CloneImage(image,image->columns-reference->columns+1,
    image->rows-reference->rows+1,MagickTrue,exception);
  if (similarity_image == (Image *) NULL)
    return((Image *) NULL);
  if (SetImageStorageClass(similarity_image,DirectClass) == MagickFalse)
    {
      InheritException(exception,&similarity_image->exception);
      similarity_image=DestroyImage(similarity_image);
      return((Image *) NULL);
    }
  (void) SetImageAlphaChannel(similarity_image,DeactivateAlphaChannel);
  /*
    Measure similarity of reference image against image.
  */
  similarity_threshold=(-1.0);
  artifact=GetImageArtifact(image,"compare:similarity-threshold");
  if (artifact != (const char *) NULL)
    similarity_threshold=StringToDouble(artifact,(char **) NULL);
  status=MagickTrue;
  progress=0;
  similarity_view=AcquireVirtualCacheView(similarity_image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) \
    shared(progress,status,similarity_metric) \
    magick_number_threads(image,image,image->rows-reference->rows+1,1)
#endif
  for (y=0; y < (ssize_t) (image->rows-reference->rows+1); y++)
  {
    double
      similarity;

    register ssize_t
      x;

    register PixelPacket
      *magick_restrict q;

    if (status == MagickFalse)
      continue;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
    #pragma omp flush(similarity_metric)
#endif
    if (*similarity_metric <= similarity_threshold)
      continue;
    q=GetCacheViewAuthenticPixels(similarity_view,0,y,similarity_image->columns,
      1,exception);
    if (q == (const PixelPacket *) NULL)
      {
        status=MagickFalse;
        continue;
      }
    for (x=0; x < (ssize_t) (image->columns-reference->columns+1); x++)
    {
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp flush(similarity_metric)
#endif
      if (*similarity_metric <= similarity_threshold)
        break;
      similarity=GetSimilarityMetric(image,reference,metric,x,y,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp critical (MagickCore_SimilarityImage)
#endif
      if ((metric == NormalizedCrossCorrelationErrorMetric) ||
          (metric == UndefinedErrorMetric))
        similarity=1.0-similarity;
      if (similarity < *similarity_metric)
        {
          *similarity_metric=similarity;
          offset->x=x;
          offset->y=y;
        }
      if (metric == PerceptualHashErrorMetric)
        similarity=MagickMin(0.01*similarity,1.0);
      SetPixelRed(q,ClampToQuantum(QuantumRange-QuantumRange*similarity));
      SetPixelGreen(q,GetPixelRed(q));
      SetPixelBlue(q,GetPixelRed(q));
      q++;
    }
    if (SyncCacheViewAuthenticPixels(similarity_view,exception) == MagickFalse)
      status=MagickFalse;
    if (image->progress_monitor != (MagickProgressMonitor) NULL)
      {
        MagickBooleanType
          proceed;

        proceed=SetImageProgress(image,SimilarityImageTag,progress++,
          image->rows);
        if (proceed == MagickFalse)
          status=MagickFalse;
      }
  }
  similarity_view=DestroyCacheView(similarity_view);
  if (status == MagickFalse)
    similarity_image=DestroyImage(similarity_image);
  return(similarity_image);
}

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <sys/time.h>

#include <PDFDoc.h>
#include <Page.h>
#include <Gfx.h>
# include <PDFDocEncoding.h>
#include <TextOutputDev.h>
#include <SplashOutputDev.h>
#include <splash/SplashBitmap.h>

#include "Epdf.h"
#include "epdf_poppler_private.h"


# include <sys/time.h>
EAPI double
ecore_time_get(void)
{
   struct timeval      timev;

   gettimeofday(&timev, NULL);
   return (double)timev.tv_sec + (((double)timev.tv_usec) / 1000000);
}

Epdf_Page *
epdf_page_new (const Epdf_Document *doc)
{
  Epdf_Page *page;

  if (!doc)
    return NULL;

  page = (Epdf_Page *)malloc (sizeof (Epdf_Page));
  if (!page)
    return NULL;

  page->doc = (Epdf_Document *)doc;
  page->page = NULL;
  page->index = 0;
  page->transition = NULL;
  page->text_dev = NULL;
  page->gfx = NULL;
  page->orientation = EPDF_PAGE_ORIENTATION_PORTRAIT;
  page->hscale = 1.0;
  page->vscale = 1.0;

  page->page = doc->pdfdoc->getCatalog ()->getPage (page->index + 1);
  if (!page->page || !page->page->isOk ()) {
    free (page);
    return NULL;
  }

  return page;
}

void
epdf_page_delete (Epdf_Page *page)
{
  if (!page)
    return;

  if (page->gfx)
    delete page->gfx;
  if (page->text_dev)
    delete page->text_dev;
  epdf_page_transition_delete (page->transition);
  free (page);
}

void
epdf_page_render (Epdf_Page *page, Evas_Object *o)
{
  SplashColor      white;
  SplashColorPtr   color_ptr;
  Epdf_Document   *doc;
  unsigned int    *m = NULL;
  double           hscale;
  double           vscale;
  int              rotate;
  int              width;
  int              height;

  double t1,t2;
  t1 = ecore_time_get();

  white[0] = 255;
  white[1] = 255;
  white[2] = 255;
  white[3] = 255;

  if (!page->page || !page->page->isOk ())
    return;

  doc = page->doc;

  switch (page->orientation) {
  case EPDF_PAGE_ORIENTATION_LANDSCAPE:
    rotate = 90;
    break;
  case EPDF_PAGE_ORIENTATION_UPSIDEDOWN:
    rotate = 180;
    break;
  case EPDF_PAGE_ORIENTATION_SEASCAPE:
    rotate = 270;
    break;
  case EPDF_PAGE_ORIENTATION_PORTRAIT:
  default:
    rotate = 0;
    break;
  }

  epdf_page_scale_get(page, &hscale, &vscale);

  SplashOutputDev output_dev(splashModeXBGR8, 4, gFalse, white);
#ifdef HAVE_POPPLER_0_20
  output_dev.startDoc(doc->pdfdoc);
  page->page->display(&output_dev,
                      72.0 * hscale,
                      72.0 * vscale,
                      rotate,
                      false, false, false,
                      NULL, NULL, NULL, NULL);
#else
  output_dev.startDoc(doc->pdfdoc->getXRef());
  page->page->display(&output_dev,
                      72.0 * hscale,
                      72.0 * vscale,
                      rotate,
                      false, false, false,
                      doc->pdfdoc->getCatalog());
#endif

  color_ptr = output_dev.getBitmap()->getDataPtr();

  width = output_dev.getBitmap()->getWidth();
  height = output_dev.getBitmap()->getHeight();

  evas_object_image_size_set(o, width, height);
  evas_object_image_fill_set(o, 0, 0, width, height);
  m = (unsigned int *)evas_object_image_data_get(o, 1);
  if (!m)
    return;

  memcpy (m, color_ptr, height * width * 4);
  evas_object_image_data_set(o, m);
  evas_object_image_data_update_add(o, 0, 0, width, height);
  evas_object_resize(o, width, height);
  //  evas_object_image_alpha_set (o, 0);

  t2 = ecore_time_get();
  printf ("time poppler: %f\n", t2-t1);
}

void
epdf_page_render_slice (Epdf_Page *page, Evas_Object *o, int x, int y, int w, int h)
{
  SplashColor      white;
  SplashColorPtr   color_ptr;
  Epdf_Document   *doc;
  unsigned int    *m = NULL;
  double           hscale;
  double           vscale;
  int              rotate;
  int              width;
  int              height;

  white[0] = 255;
  white[1] = 255;
  white[2] = 255;
  white[3] = 255;

  if (!page->page || !page->page->isOk ())
    return;

  doc = page->doc;

  switch (page->orientation) {
  case EPDF_PAGE_ORIENTATION_LANDSCAPE:
    rotate = 90;
    break;
  case EPDF_PAGE_ORIENTATION_UPSIDEDOWN:
    rotate = 180;
    break;
  case EPDF_PAGE_ORIENTATION_SEASCAPE:
    rotate = 270;
    break;
  case EPDF_PAGE_ORIENTATION_PORTRAIT:
  default:
    rotate = 0;
    break;
  }

  epdf_page_scale_get(page, &hscale, &vscale);

  SplashOutputDev output_dev(splashModeXBGR8, 4, gFalse, white);
#ifdef HAVE_POPPLER_0_20
  output_dev.startDoc(doc->pdfdoc);
  page->page->displaySlice(&output_dev, 72.0 * hscale, 72.0 * vscale,
                           rotate,
                           false, false,
                           x, y, w, h,
                           false,
                           NULL, NULL, NULL, NULL);
#else
  output_dev.startDoc(doc->pdfdoc->getXRef());
  page->page->displaySlice(&output_dev, 72.0 * hscale, 72.0 * vscale,
                           rotate,
                           false, false,
                           x, y, w, h,
                           false,
                           doc->pdfdoc->getCatalog ());
#endif

  color_ptr = output_dev.getBitmap()->getDataPtr();

  width = output_dev.getBitmap()->getWidth();
  height = output_dev.getBitmap()->getHeight();

  evas_object_image_size_set(o, width, height);
  evas_object_image_fill_set(o, 0, 0, width, height);
//   evas_object_image_data_set(o, color_ptr);
  m = (unsigned int *)evas_object_image_data_get(o, 1);
  if (!m)
    return;

  memcpy (m, color_ptr, height * width * 4);
  evas_object_image_data_update_add(o, 0, 0, width, height);
  evas_object_resize(o, width, height);
  //  evas_object_image_alpha_set (o, 0);
}

void
epdf_page_page_set (Epdf_Page *page, int p)
{
  if (!page)
    return;

  page->page = page->doc->pdfdoc->getCatalog ()->getPage (p + 1);
}

int
epdf_page_page_get (const Epdf_Page *page)
{
  if (!page)
    return 0;

  return page->page->getNum () - 1;
}

static TextOutputDev *
epdf_page_text_output_dev_get (Epdf_Page *page)
{
  if (!page)
    return NULL;

  if (!page->text_dev)
    {
#ifdef HAVE_POPPLER_0_20
      page->text_dev = new TextOutputDev(NULL, gTrue, 0, gFalse, gFalse);
      page->gfx = page->page->createGfx(page->text_dev,
                                        72.0, 72.0, 0,
                                        gFalse, /* useMediaBox */
                                        gTrue, /* Crop */
                                        -1, -1, -1, -1,
                                        gFalse, /* printing */
                                        NULL, NULL);
#else
      page->text_dev = new TextOutputDev(NULL, 1, 0, 0);
      page->gfx = page->page->createGfx(page->text_dev,
                                        72.0, 72.0, 0,
                                        gFalse, /* useMediaBox */
                                        gTrue, /* Crop */
                                        -1, -1, -1, -1,
                                        gFalse, /* printing */
                                        page->doc->pdfdoc->getCatalog (),
                                        NULL, NULL, NULL, NULL);
#endif

      page->page->display(page->gfx);

      page->text_dev->endPage();
    }

  return page->text_dev;
}

char *
epdf_page_text_get (Epdf_Page *page, Epdf_Rectangle r)
{
  TextOutputDev *text_dev;
  GooString     *sel_text;
  char          *result;
  PDFRectangle   pdf_selection;

  if (!page)
    return NULL;

  text_dev = epdf_page_text_output_dev_get (page);

  pdf_selection.x1 = r.x1;
  pdf_selection.y1 = r.y1;
  pdf_selection.x2 = r.x2;
  pdf_selection.y2 = r.y2;

  sel_text = new GooString;
  /* added selectionStyleGlyph to catch up with poppler 0.6. Is that correct
     or should we rather use selectionStyleLine or selectionStyleWord? :M: */
  sel_text = text_dev->getSelectionText (&pdf_selection, selectionStyleGlyph);
  result = strdup (sel_text->getCString ());
  delete sel_text;

  return result;
}

Eina_List *
epdf_page_text_find (const Epdf_Page *page,
                     const char      *text,
                     unsigned char    is_case_sensitive)
{
  Epdf_Rectangle *match;
  Eina_List      *matches = NULL;
  double          xMin, yMin, xMax, yMax;
  int             length;
  int             height;


  if (!page || !text)
    return NULL;

  GooString tmp (text);
  Unicode *s;

  {
    length = tmp.getLength();
    s = (Unicode *)gmallocn(length, sizeof(Unicode));
    bool anyNonEncoded = false;
    for (int j = 0; j < length && !anyNonEncoded; ++j) {
      s[j] = pdfDocEncoding[tmp.getChar(j) & 0xff];
      if (!s[j]) anyNonEncoded = true;
    }
    if ( anyNonEncoded )
      {
	for (int j = 0; j < length; ++j) {
	  s[j] = tmp.getChar(j);
	}
      }
  }

  length = strlen (text);
  epdf_page_size_get(page, NULL, &height);
  xMin = 0;
  yMin = 0;

#ifdef HAVE_POPPLER_0_20
  TextOutputDev output_dev(NULL, gTrue, 0, gFalse, gFalse);
  page->page->display(&output_dev, 72, 72, 0, false,
                      true, false,
                      NULL, NULL, NULL, NULL);
#warning you probably want to add backwards as parameters
  while (output_dev.findText(s, tmp.getLength (),
                             gFalse, gTrue, // startAtTop, stopAtBottom
                             gTrue, gFalse, // startAtLast, stopAtLast
                             is_case_sensitive, gFalse, // caseSensitive, backwards
                             gFalse, // wholeWord
                             &xMin, &yMin, &xMax, &yMax))
#else
  TextOutputDev output_dev(NULL, 1, 0, 0);
  page->page->display(&output_dev, 72, 72, 0, false,
                      true, false,
                      page->doc->pdfdoc->getCatalog());
#warning you probably want to add backwards as parameters
  while (output_dev.findText(s, tmp.getLength (),
                             0, 1, // startAtTop, stopAtBottom
                             1, 0, // startAtLast, stopAtLast
                             is_case_sensitive, 0, // caseSensitive, backwards
                             &xMin, &yMin, &xMax, &yMax))
#endif
    {
      match = (Epdf_Rectangle *)malloc (sizeof (Epdf_Rectangle));
      match->x1 = xMin;
      match->y1 = yMin;//height - yMax;
      match->x2 = xMax;
      match->y2 = yMax;//height - yMin;
      matches = eina_list_append (matches, match);
    }

  return matches;
}

Epdf_Page_Transition *
epdf_page_transition_get (const Epdf_Page *page)
{
  if (!page)
    return NULL;

  return page->transition;
}

void
epdf_page_size_get (const Epdf_Page *page, int *width, int *height)
{
  int rotate;
  int w = 0;
  int h = 0;

  if (page) {
    PDFRectangle box;
    GBool crop;

    rotate = page->page->getRotate ();
    page->page->makeBox(72, 72, 0, false, false,
			-1, -1, -1, -1, &box, &crop);

    if (rotate == 90 || rotate == 270) {
      h = box.x2 - box.x1;
      w = box.y2 - box.y1;
    }
    else {
      w = box.x2 - box.x1;
      h = box.y2 - box.y1;
    }
  }

  if (width) *width = w;
  if (height) *height = h;
}

void
epdf_page_scale_set (Epdf_Page *page, double hscale, double vscale)
{
  if (!page)
    return;

  if (hscale != page->hscale) page->hscale = hscale;
  if (vscale != page->vscale) page->vscale = vscale;
}

void
epdf_page_scale_get (const Epdf_Page *page, double *hscale, double *vscale)
{
  if (hscale) *hscale = page ? page->hscale : 1.0;
  if (vscale) *vscale = page ? page->vscale : 1.0;
}

void
epdf_page_orientation_set (Epdf_Page            *page,
                           Epdf_Page_Orientation orientation)
{
  if (!page)
    return;

  if (page->orientation != orientation)
    page->orientation = orientation;
}

Epdf_Page_Orientation
epdf_page_orientation_get (const Epdf_Page *page)
{
  if (!page)
    return EPDF_PAGE_ORIENTATION_PORTRAIT;

  return page->orientation;
}

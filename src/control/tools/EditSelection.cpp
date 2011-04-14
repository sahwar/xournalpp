#include "EditSelection.h"
#include "../../undo/UndoRedoHandler.h"
#include "../../model/Element.h"
#include "../../model/Stroke.h"
#include "../../model/Text.h"
#include "Selection.h"
#include "../../gui/PageView.h"
#include "../../view/DocumentView.h"
#include "../../undo/SizeUndoAction.h"
#include "../../undo/ColorUndoAction.h"
#include "../../undo/FontUndoAction.h"
#include "../../gui/XournalView.h"
#include "../../gui/pageposition/PagePositionHandler.h"
#include "../../control/Control.h"
#include "../../model/Document.h"
#include "../../model/Layer.h"
#include "../../util/ObjectStream.h"

#include "EditSelectionContents.h"


EditSelection::EditSelection(UndoRedoHandler * undo, PageRef page, PageView * view) {
	XOJ_INIT_TYPE(EditSelection);

	this->x = 0;
	this->y = 0;
	this->width = 0;
	this->height = 0;

	contstruct(undo, view, page);
}

EditSelection::EditSelection(UndoRedoHandler * undo, Selection * selection, PageView * view) {
	XOJ_INIT_TYPE(EditSelection);

	selection->getSelectedRect(this->x, this->y, this->width, this->height);

	contstruct(undo, view, view->getPage());

	for (GList * l = selection->selectedElements; l != NULL; l = l->next) {
		Element * e = (Element *) l->data;
		this->sourceLayer->removeElement(e, false);
		addElement(e);
	}

	view->rerenderPage();
}

EditSelection::EditSelection(UndoRedoHandler * undo, Element * e, PageView * view, PageRef page) {
	XOJ_INIT_TYPE(EditSelection);

	this->x = e->getX();
	this->y = e->getY();
	this->width = e->getElementWidth();
	this->height = e->getElementHeight();

	contstruct(undo, view, page);

	addElement(e);
	this->sourceLayer->removeElement(e, false);

	view->rerenderElement(e);
}

/**
 * Our internal constructor
 */
void EditSelection::contstruct(UndoRedoHandler * undo, PageView * view, PageRef sourcePage) {
	XOJ_CHECK_TYPE(EditSelection);

	this->view = view;
	this->undo = undo;
	this->sourcePage = sourcePage;
	this->sourceLayer = this->sourcePage.getSelectedLayer();

	this->aspectRatio = false;

	this->relMousePosX = 0;
	this->relMousePosY = 0;
	this->mouseDownType = CURSOR_SELECTION_NONE;

	this->contents = new EditSelectionContents(this->x, this->y, this->width, this->height, this->sourcePage, this->sourceLayer, this->view);
}

EditSelection::~EditSelection() {
	XOJ_CHECK_TYPE(EditSelection);

	finalizeSelection();

	this->sourcePage = NULL;
	this->sourceLayer = NULL;

	delete this->contents;
	this->contents = NULL;

	this->view = NULL;
	this->undo = NULL;

	XOJ_RELEASE_TYPE(EditSelection);
}

/**
 * Finishes all pending changes, move the elements, scale the elements and add
 * them to new layer if any or to the old if no new layer
 */
void EditSelection::finalizeSelection() {
	XOJ_CHECK_TYPE(EditSelection);

	PageView * v = getBestMatchingPageView();
	if (v == NULL) {
		this->view->getXournal()->deleteSelection(this);
	} else {
		PageRef page = this->view->getPage();
		Layer * layer = page.getSelectedLayer();
		this->contents->finalizeSelection(this->x, this->y, this->width, this->height, this->aspectRatio, layer, page, this->view, this->undo);

		this->view->rerenderRect(this->x, this->y, this->width, this->height);

		// This is needed if the selection not was 100% on a page
		this->view->getXournal()->repaintSelection(true);
	}
}

/**
 * get the X coordinate relative to the provided view (getView())
 * in document coordinates
 */
double EditSelection::getXOnView() {
	XOJ_CHECK_TYPE(EditSelection);

	return this->x;
}

/**
 * get the Y coordinate relative to the provided view (getView())
 * in document coordinates
 */
double EditSelection::getYOnView() {
	XOJ_CHECK_TYPE(EditSelection);

	return this->y;
}

/**
 * get the width in document coordinates (multiple with zoom)
 */
double EditSelection::getWidth() {
	XOJ_CHECK_TYPE(EditSelection);

	return this->width;
}

/**
 * get the height in document coordinates (multiple with zoom)
 */
double EditSelection::getHeight() {
	XOJ_CHECK_TYPE(EditSelection);

	return this->height;
}

/**
 * get the source page (where the selection was done)
 */
PageRef EditSelection::getSourcePage() {
	XOJ_CHECK_TYPE(EditSelection);

	return this->sourcePage;
}

/**
 * Get the X coordinate in View coordinates (absolute)
 */
int EditSelection::getXOnViewAbsolute() {
	XOJ_CHECK_TYPE(EditSelection);

	double zoom = view->getXournal()->getZoom();
	return this->view->getX() + this->getXOnView() * zoom;
}

/**
 * Get the Y coordinate in View coordinates (absolute)
 */
int EditSelection::getYOnViewAbsolute() {
	XOJ_CHECK_TYPE(EditSelection);

	double zoom = view->getXournal()->getZoom();
	return this->view->getY() + this->getYOnView() * zoom;
}

/**
 * Get the width in View coordinates
 */
int EditSelection::getViewWidth() {
	XOJ_CHECK_TYPE(EditSelection);

	double zoom = view->getXournal()->getZoom();
	return this->width * zoom;
}

/**
 * Get the height in View coordinates
 */
int EditSelection::getViewHeight() {
	XOJ_CHECK_TYPE(EditSelection);

	double zoom = view->getXournal()->getZoom();
	return this->height * zoom;
}

/**
 * Sets the tool size for pen or eraser, returs an undo action
 * (or NULL if nothing is done)
 */
UndoAction * EditSelection::setSize(ToolSize size, const double * thiknessPen, const double * thiknessHilighter, const double * thiknessEraser) {
	XOJ_CHECK_TYPE(EditSelection);

	return this->contents->setSize(size, thiknessPen, thiknessHilighter, thiknessEraser);
}

/**
 * Set the color of all elements, return an undo action
 * (Or NULL if nothing done, e.g. because there is only an image)
 */
UndoAction * EditSelection::setColor(int color) {
	XOJ_CHECK_TYPE(EditSelection);

	return this->contents->setColor(color);
}

/**
 * Sets the font of all containing text elements, return an undo action
 * (or NULL if there are no Text elements)
 */
UndoAction * EditSelection::setFont(XojFont & font) {
	XOJ_CHECK_TYPE(EditSelection);

	return this->contents->setFont(font);
}

/**
 * Fills de undo item if the selection is deleted
 * the selection is cleared after
 */
void EditSelection::fillUndoItem(DeleteUndoAction * undo) {
	this->contents->fillUndoItem(undo);
}

/**
 * Add an element to the this selection
 */
void EditSelection::addElement(Element * e) {
	XOJ_CHECK_TYPE(EditSelection);

	this->contents->addElement(e);

	if (e->rescaleOnlyAspectRatio()) {
		this->aspectRatio = true;
	}
}

/**
 * Returns all containig elements of this selections
 */
ListIterator<Element *> EditSelection::getElements() {
	XOJ_CHECK_TYPE(EditSelection);

	return this->contents->getElements();
}

/**
 * Finish the current movement
 * (should be called in the mouse-button-released event handler)
 */
void EditSelection::mouseUp() {
	XOJ_CHECK_TYPE(EditSelection);

	this->mouseDownType = CURSOR_SELECTION_NONE;
}

/**
 * Handles mouse input for moving and resizing, coordinates are relative to "view"
 */
void EditSelection::mouseDown(CursorSelectionType type, double x, double y) {
	double zoom = this->view->getXournal()->getZoom();
	x /= zoom;
	y /= zoom;

	this->mouseDownType = type;
	this->relMousePosX = x - this->x;
	this->relMousePosY = y - this->y;
}

/**
 * Handles mouse input for moving and resizing, coordinates are relative to "view"
 */
void EditSelection::mouseMove(double x, double y) {
	double zoom = this->view->getXournal()->getZoom();
	x /= zoom;
	y /= zoom;

	if (this->mouseDownType == CURSOR_SELECTION_MOVE) {
		this->x = x - this->relMousePosX;
		this->y = y - this->relMousePosY;

		this->view->getXournal()->repaintSelection();
	} else if (this->mouseDownType == CURSOR_SELECTION_TOP_LEFT) {
		double dx = x - this->x;
		double dy = y - this->y;
		double f;
		if (ABS(dy) < ABS(dx)) {
			f = (this->height + dy) / this->height;
		} else {
			f = (this->width + dx) / this->width;
		}

		double oldW = this->width;
		double oldH = this->height;
		this->width /= f;
		this->height /= f;

		this->x += oldW - this->width;
		this->y += oldH - this->height;

		this->view->getXournal()->repaintSelection();
	} else if (this->mouseDownType == CURSOR_SELECTION_TOP_RIGHT) {
		double dx = x - this->x - this->width;
		double dy = y - this->y;
		double f;
		if (ABS(dy) < ABS(dx)) {
			f = this->height / (this->height + dy);
		} else {
			f = (this->width + dx) / this->width;
		}

		double oldH = this->height;
		this->width *= f;
		this->height *= f;

		this->y += oldH - this->height;

		this->view->getXournal()->repaintSelection();
	} else if (this->mouseDownType == CURSOR_SELECTION_BOTTOM_LEFT) {
		double dx = x - this->x;
		double dy = y - this->y - this->height;
		double f;
		if (ABS(dy) < ABS(dx)) {
			f = (this->height + dy) / this->height;
		} else {
			f = this->width / (this->width + dx);
		}

		double oldW = this->width;
		this->width *= f;
		this->height *= f;

		this->x += oldW - this->width;

		this->view->getXournal()->repaintSelection();
	} else if (this->mouseDownType == CURSOR_SELECTION_BOTTOM_RIGHT) {
		double dx = x - this->x - this->width;
		double dy = y - this->y - this->height;
		double f;
		if (ABS(dy) < ABS(dx)) {
			f = (this->height + dy) / this->height;
		} else {
			f = (this->width + dx) / this->width;
		}

		this->width *= f;
		this->height *= f;

		this->view->getXournal()->repaintSelection();
	} else if (this->mouseDownType == CURSOR_SELECTION_TOP) {
		double dy = y - this->y;
		this->height -= dy;
		this->y += dy;
		this->view->getXournal()->repaintSelection();
	} else if (this->mouseDownType == CURSOR_SELECTION_BOTTOM) {
		double dy = y - this->y - this->height;
		this->height += dy;
		this->view->getXournal()->repaintSelection();
	} else if (this->mouseDownType == CURSOR_SELECTION_LEFT) {
		double dx = x - this->x;
		this->width -= dx;
		this->x += dx;

		this->view->getXournal()->repaintSelection();
	} else if (this->mouseDownType == CURSOR_SELECTION_RIGHT) {
		double dx = x - this->x - this->width;
		this->width += dx;
		this->view->getXournal()->repaintSelection();
	}

	PageView * v = getBestMatchingPageView();

	if (v && v != this->view) {
		XournalView * xournal = this->view->getXournal();
		int pageNr = xournal->getControl()->getDocument()->indexOf(v->getPage());

		xournal->pageSelected(pageNr);

		translateToView(v);
	}
}

PageView * EditSelection::getBestMatchingPageView() {
	PagePositionHandler * pp = this->view->getXournal()->getPagePositionHandler();
	int rx = this->getXOnViewAbsolute();
	int ry = this->getYOnViewAbsolute();
	PageView * v = pp->getBestMatchingView(rx, ry, this->getViewWidth(), this->getViewHeight());
	return v;
}

/**
 * Translate all coordinates which are relative to the current view to the new view,
 * and set the attribute view to the new view
 */
void EditSelection::translateToView(PageView * v) {
	double zoom = view->getXournal()->getZoom();

	int aX1 = getXOnViewAbsolute();
	int aY1 = getYOnViewAbsolute();

	this->x = (aX1 - v->getX()) / zoom;
	this->y = (aY1 - v->getY()) / zoom;

	this->view = v;

	int aX2 = getXOnViewAbsolute();
	int aY2 = getYOnViewAbsolute();

	if (aX1 != aX2) {
		printf("aX1 != aX2!! %i / %i\n", aX1, aX2);
	}
	if (aY1 != aY2) {
		printf("aY1 != aY2!! %i / %i\n", aY1, aY2);
	}
}

/**
 * If the selection should moved (or rescaled)
 */
bool EditSelection::isMoving() {
	return this->mouseDownType != CURSOR_SELECTION_NONE;
}

/**
 * Move the selection
 */
void EditSelection::moveSelection(double dx, double dy) {
	XOJ_CHECK_TYPE(EditSelection);

	this->x -= dx;
	this->y -= dy;

	ensureWithinVisibleArea();

	int x = this->view->getX();
	int y = this->view->getY();
	double zoom = this->view->getXournal()->getZoom();
	this->view->getXournal()->ensureRectIsVisible(x, y, this->width * zoom, this->height * zoom);

	this->view->getXournal()->repaintSelection();
}

/**
 * If the selection is outside the visible area correct the coordinates
 */
void EditSelection::ensureWithinVisibleArea() {
	XOJ_CHECK_TYPE(EditSelection);

	//TODO: scroll to this point if not in visible area

	//	double zoom = this->view->getXournal()->getZoom();
	//	int x = this->view->getX() - this->offsetX + this->relativeX;
	//	if (x < 0) {
	//		this->offsetX += x;
	//	}
	//	int maxX = this->view->getXournal()->getMaxAreaX();
	//	if (maxX < x + this->width * zoom) {
	//		this->offsetX += (x + this->width * zoom) - maxX;
	//	}
	//
	//	int y = this->view->getY() - this->offsetY + this->relativeY;
	//	if (y < 0) {
	//		this->offsetY += y;
	//	}
	//	int maxY = this->view->getXournal()->getMaxAreaY();
	//	if (maxY < y + this->height * zoom) {
	//		this->offsetY += (y + this->height * zoom) - maxY;
	//	}
}

/**
 * Get the cursor type for the current position (if 0 then the default cursor should be used)
 */
CursorSelectionType EditSelection::getSelectionTypeForPos(double x, double y, double zoom) {
	XOJ_CHECK_TYPE(EditSelection);

	double x1 = getXOnView() * zoom;
	double x2 = x1 + (this->width * zoom);
	double y1 = getYOnView() * zoom;
	double y2 = y1 + (this->height * zoom);

	const int EDGE_PADDING = 5;
	const int BORDER_PADDING = 3;

	if (x1 - EDGE_PADDING <= x && x <= x1 + EDGE_PADDING && y1 - EDGE_PADDING <= y && y <= y1 + EDGE_PADDING) {
		return CURSOR_SELECTION_TOP_LEFT;
	}

	if (x2 - EDGE_PADDING <= x && x <= x2 + EDGE_PADDING && y1 - EDGE_PADDING <= y && y <= y1 + EDGE_PADDING) {
		return CURSOR_SELECTION_TOP_RIGHT;
	}

	if (x1 - EDGE_PADDING <= x && x <= x1 + EDGE_PADDING && y2 - EDGE_PADDING <= y && y <= y2 + EDGE_PADDING) {
		return CURSOR_SELECTION_BOTTOM_LEFT;
	}

	if (x2 - EDGE_PADDING <= x && x <= x2 + EDGE_PADDING && y2 - EDGE_PADDING <= y && y <= y2 + EDGE_PADDING) {
		return CURSOR_SELECTION_BOTTOM_RIGHT;
	}

	if (!this->aspectRatio) {
		if (x1 <= x && x2 >= x) {
			if (y1 - BORDER_PADDING <= y && y <= y1 + BORDER_PADDING) {
				return CURSOR_SELECTION_TOP;
			}

			if (y2 - BORDER_PADDING <= y && y <= y2 + BORDER_PADDING) {
				return CURSOR_SELECTION_BOTTOM;
			}
		}

		if (y1 <= y && y2 >= y) {
			if (x1 - BORDER_PADDING <= x && x <= x1 + BORDER_PADDING) {
				return CURSOR_SELECTION_LEFT;
			}

			if (x2 - BORDER_PADDING <= x && x <= x2 + BORDER_PADDING) {
				return CURSOR_SELECTION_RIGHT;
			}
		}
	}

	if (x1 <= x && x <= x2 && y1 <= y && y <= y2) {
		return CURSOR_SELECTION_MOVE;
	}

	return CURSOR_SELECTION_NONE;
}

/**
 * Paints the selection to cr, with the given zoom factor. The coordinates of cr
 * should be relative to the provideded view by getView() (use translateEvent())
 */
void EditSelection::paint(cairo_t * cr, double zoom) {
	XOJ_CHECK_TYPE(EditSelection);

	double x = this->x;
	double y = this->y;

	this->contents->paint(cr, x, y, this->width, this->height, zoom);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	GdkColor selectionColor = view->getSelectionColor();

	// set the line always the same size on display
	cairo_set_line_width(cr, 1);

	const double dashes[] = { 10.0, 10.0 };
	cairo_set_dash(cr, dashes, sizeof(dashes) / sizeof(dashes[0]), 0);
	cairo_set_source_rgb(cr, selectionColor.red / 65536.0, selectionColor.green / 65536.0, selectionColor.blue / 65536.0);

	cairo_rectangle(cr, x * zoom, y * zoom, width * zoom, height * zoom);

	cairo_stroke_preserve(cr);
	cairo_set_source_rgba(cr, selectionColor.red / 65536.0, selectionColor.green / 65536.0, selectionColor.blue / 65536.0, 0.3);
	cairo_fill(cr);

	cairo_set_dash(cr, NULL, 0, 0);

	if (!this->aspectRatio) {
		// top
		drawAnchorRect(cr, x + width / 2, y, zoom);
		// bottom
		drawAnchorRect(cr, x + width / 2, y + height, zoom);
		// left
		drawAnchorRect(cr, x, y + height / 2, zoom);
		// right
		drawAnchorRect(cr, x + width, y + height / 2, zoom);
	}

	// top left
	drawAnchorRect(cr, x, y, zoom);
	// top right
	drawAnchorRect(cr, x + width, y, zoom);
	// bottom left
	drawAnchorRect(cr, x, y + height, zoom);
	// bottom right
	drawAnchorRect(cr, x + width, y + height, zoom);
}

/**
 * draws an idicator where you can scale the selection
 */
void EditSelection::drawAnchorRect(cairo_t * cr, double x, double y, double zoom) {
	XOJ_CHECK_TYPE(EditSelection);

	GdkColor selectionColor = view->getSelectionColor();
	cairo_set_source_rgb(cr, selectionColor.red / 65536.0, selectionColor.green / 65536.0, selectionColor.blue / 65536.0);
	cairo_rectangle(cr, x * zoom - 4, y * zoom - 4, 8, 8);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
}

PageView * EditSelection::getView() {
	XOJ_CHECK_TYPE(EditSelection);

	return this->view;
}

void EditSelection::serialize(ObjectOutputStream & out) {
	out.writeObject("EditSelection");

	out.writeDouble(this->x);
	out.writeDouble(this->y);
	out.writeDouble(this->width);
	out.writeDouble(this->height);

	out << this->contents;
	out.endObject();

	ListIterator<Element *> it = this->getElements();
	int count = it.getLength();
	out.writeInt(count);

	while (it.hasNext()) {
		Element * e = it.next();
		out << e;
	}
}

void EditSelection::readSerialized(ObjectInputStream & in) throw (InputStreamException) {
	in.readObject("EditSelection");
	this->x = in.readDouble();
	this->y = in.readDouble();
	this->width = in.readDouble();
	this->height = in.readDouble();

	in >> this->contents;

	in.endObject();
}


#include <QEvent>
#include <GL\glew.h>
#include "Viewer2d.h"
#include "cloth\clothManager.h"
#include "cloth\clothPiece.h"
#include "Renderable\ObjMesh.h"
#include "cloth\panelPolygon.h"
#include "../clothdesigner.h"
#include "Sewing2dPatternEventHandle.h"
Sewing2dPatternEventHandle::Sewing2dPatternEventHandle(Viewer2d* v)
: Abstract2dEventHandle(v)
{
	QString name = "icons/pattern_sewing.png";
	QPixmap img(name);
	img = img.scaledToWidth(32, Qt::TransformationMode::SmoothTransformation);
	m_cursor = QCursor(img, 1, 1);
	m_iconFile = name;
	m_inactiveIconFile = name;
	m_toolTips = "edit pattern";
}

Sewing2dPatternEventHandle::~Sewing2dPatternEventHandle()
{

}

void Sewing2dPatternEventHandle::handleEnter()
{
	Abstract2dEventHandle::handleEnter();
	m_viewer->setFocus();
	m_viewer->beginSewingMode();
}
void Sewing2dPatternEventHandle::handleLeave()
{
	m_viewer->endSewingMode();
	m_viewer->clearFocus();
	m_viewer->endDragBox();
	Abstract2dEventHandle::handleLeave();
}

void Sewing2dPatternEventHandle::mousePressEvent(QMouseEvent *ev)
{
	Abstract2dEventHandle::mousePressEvent(ev);

	if (ev->buttons() == Qt::LeftButton)
	{
		pick(ev->pos());
		if (pickInfo().renderId == 0)
			m_viewer->beginDragBox(ev->pos());
	} // end if left button
}

void Sewing2dPatternEventHandle::mouseReleaseEvent(QMouseEvent *ev)
{
	auto manager = m_viewer->getManager();
	if (manager == nullptr)
		return;

	if (m_viewer->buttons() & Qt::LeftButton)
	{
		auto op = ldp::AbstractPanelObject::SelectThis;
		if (ev->modifiers() & Qt::SHIFT)
			op = ldp::AbstractPanelObject::SelectUnion;
		if (ev->modifiers() & Qt::CTRL)
			op = ldp::AbstractPanelObject::SelectUnionInverse;
		if (ev->pos() == m_mouse_press_pt)
		{
			bool changed = false;
			for (size_t iSewing = 0; iSewing < manager->numSewings(); iSewing++)
			if (manager->sewing(iSewing)->select(pickInfo().renderId, op))
				changed = true;
			if (m_viewer->getMainUI() && changed)
				m_viewer->getMainUI()->pushHistory(QString().sprintf("sew select: %d",
				pickInfo().renderId), ldp::HistoryStack::TypePatternSelect);
		}
		else
		{
			const QImage& I = m_viewer->fboImage();
			std::set<int> ids;
			float x0 = std::max(0, std::min(m_mouse_press_pt.x(), ev->pos().x()));
			float x1 = std::min(I.width() - 1, std::max(m_mouse_press_pt.x(), ev->pos().x()));
			float y0 = std::max(0, std::min(m_mouse_press_pt.y(), ev->pos().y()));
			float y1 = std::min(I.height() - 1, std::max(m_mouse_press_pt.y(), ev->pos().y()));
			for (int y = y0; y <= y1; y++)
			for (int x = x0; x <= x1; x++)
				ids.insert(m_viewer->fboRenderedIndex(QPoint(x, y)));
			bool changed = false;
			for (size_t iSewing = 0; iSewing < manager->numSewings(); iSewing++)
			if (manager->sewing(iSewing)->select(ids, op))
				changed = true;
			if (m_viewer->getMainUI() && changed)
				m_viewer->getMainUI()->pushHistory(QString().sprintf("sew select: %d",
				pickInfo().renderId), ldp::HistoryStack::TypePatternSelect);
		}
	}
	m_viewer->endDragBox();
	Abstract2dEventHandle::mouseReleaseEvent(ev);
}

void Sewing2dPatternEventHandle::mouseDoubleClickEvent(QMouseEvent *ev)
{
	Abstract2dEventHandle::mouseDoubleClickEvent(ev);
}

void Sewing2dPatternEventHandle::mouseMoveEvent(QMouseEvent *ev)
{
	Abstract2dEventHandle::mouseMoveEvent(ev);
}

void Sewing2dPatternEventHandle::wheelEvent(QWheelEvent *ev)
{
	Abstract2dEventHandle::wheelEvent(ev);
}

void Sewing2dPatternEventHandle::keyPressEvent(QKeyEvent *ev)
{
	Abstract2dEventHandle::keyPressEvent(ev);
	auto manager = m_viewer->getManager();
	if (!manager)
		return;
	ldp::AbstractPanelObject::SelectOp op = ldp::AbstractPanelObject::SelectEnd;
	switch (ev->key())
	{
	default:
		break;
	case Qt::Key_A:
		if (ev->modifiers() == Qt::CTRL)
			op = ldp::AbstractPanelObject::SelectAll;
		break;
	case Qt::Key_D:
		if (ev->modifiers() == Qt::CTRL)
			op = ldp::AbstractPanelObject::SelectNone;
		break;
	case Qt::Key_I:
		if (ev->modifiers() == (Qt::CTRL | Qt::SHIFT))
			op = ldp::AbstractPanelObject::SelectInverse;
		break;
	}

	bool changed = false;
	for (size_t iSewing = 0; iSewing < manager->numSewings(); iSewing++)
	if (manager->sewing(iSewing)->select(0, op))
		changed = true;

	if (m_viewer->getMainUI() && changed)
		m_viewer->getMainUI()->pushHistory(QString().sprintf("sew select: all(%d)",
		op), ldp::HistoryStack::TypePatternSelect);
}

void Sewing2dPatternEventHandle::keyReleaseEvent(QKeyEvent *ev)
{
	Abstract2dEventHandle::keyReleaseEvent(ev);
}

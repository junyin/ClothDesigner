#include <QEvent>
#include <GL\glew.h>
#include "Viewer2d.h"
#include "cloth\clothManager.h"
#include "cloth\clothPiece.h"
#include "cloth\HistoryStack.h"
#include "cloth\panelPolygon.h"
#include "cloth\TransformInfo.h"
#include "..\clothdesigner.h"

#include "Transform2dPatternEventHandle.h"

Transform2dPatternEventHandle::Transform2dPatternEventHandle(Viewer2d* v)
: Abstract2dEventHandle(v)
{
	QString name = "icons/pattern_transform.png";
	QPixmap img(name);
	img = img.scaledToWidth(32, Qt::TransformationMode::SmoothTransformation);
	m_cursor = QCursor(img, 1, 1);
	m_iconFile = name;
	m_inactiveIconFile = name;
	m_toolTips = "transform pattern";
}

Transform2dPatternEventHandle::~Transform2dPatternEventHandle()
{

}

void Transform2dPatternEventHandle::handleEnter()
{
	Abstract2dEventHandle::handleEnter();
	m_viewer->setFocus();
	m_transformed = false;
}

void Transform2dPatternEventHandle::handleLeave()
{
	m_viewer->clearFocus();
	m_viewer->endDragBox();
	Abstract2dEventHandle::handleLeave();
	m_transformed = false;
}

void Transform2dPatternEventHandle::mousePressEvent(QMouseEvent *ev)
{
	Abstract2dEventHandle::mousePressEvent(ev);

	auto manager = m_viewer->getManager();
	if (manager == nullptr)
		return;

	// handle selection for all buttons
	if (ev->buttons() != Qt::MidButton)
	{
		pick(ev->pos());
		if (pickInfo().renderId == 0)
			m_viewer->beginDragBox(ev->pos());
	}

	// translation start positon
	ldp::Float3 p3(ev->x(), m_viewer->height() - 1 - ev->y(), 1);
	p3 = m_viewer->camera().getWorldCoords(p3);
	m_translateStart = ldp::Float2(p3[0], p3[1]);

	// rotation center postion
	ldp::Float2 bMin = FLT_MAX, bMax = -FLT_MAX;
	for (size_t iPiece = 0; iPiece < manager->numClothPieces(); iPiece++)
	{
		auto piece = manager->clothPiece(iPiece);
		auto& panel = piece->panel();
		if (pickInfo().renderId == panel.getId() || panel.isSelected())
		{
			for (int k = 0; k < 3; k++)
			{
				bMin[k] = std::min(bMin[k], panel.bound()[0][k]);
				bMax[k] = std::max(bMax[k], panel.bound()[1][k]);
			}
		}
	} // end for iPiece
	m_rotateCenter = (bMin + bMax) / 2.f;
}

void Transform2dPatternEventHandle::mouseReleaseEvent(QMouseEvent *ev)
{
	auto manager = m_viewer->getManager();
	if (manager == nullptr)
		return;

	// handle transfrom triangulation update
	if (m_viewer->getMainUI() && m_transformed)
	{
		m_viewer->getManager()->triangulate();
		m_viewer->getMainUI()->pushHistory(QString().sprintf("pattern transform: %d",
			pickInfo().renderId), ldp::HistoryStack::TypeGeneral);
		m_transformed = false;
	}

	// handle selection ------------------------------------------------------------------
	auto op = ldp::AbstractPanelObject::SelectThis;
	if (ev->modifiers() & Qt::SHIFT)
		op = ldp::AbstractPanelObject::SelectUnion;
	if (ev->modifiers() & Qt::CTRL)
		op = ldp::AbstractPanelObject::SelectUnionInverse;
	if (ev->pos() == m_mouse_press_pt)
	{
		bool changed = false;
		for (size_t iPiece = 0; iPiece < manager->numClothPieces(); iPiece++)
		{
			auto piece = manager->clothPiece(iPiece);
			auto& panel = piece->panel();
			if (panel.select(pickInfo().renderId, op))
				changed = true;
		} // end for iPiece
		if (m_viewer->getMainUI() && changed)
			m_viewer->getMainUI()->pushHistory(QString().sprintf("pattern select: %d",
			pickInfo().renderId), ldp::HistoryStack::TypePatternSelect);
	} // end if single selection
	else if (m_viewer->isDragBoxMode())
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
		for (size_t iPiece = 0; iPiece < manager->numClothPieces(); iPiece++)
		{
			auto piece = manager->clothPiece(iPiece);
			auto& panel = piece->panel();
			if (panel.select(ids, op))
				changed = true;
		} // end for iPiece
		if (m_viewer->getMainUI() && changed)
			m_viewer->getMainUI()->pushHistory(QString().sprintf("pattern select: %d...",
			*ids.begin()), ldp::HistoryStack::TypePatternSelect);
	} // end else group selection

	m_viewer->endDragBox();
	Abstract2dEventHandle::mouseReleaseEvent(ev);
}

void Transform2dPatternEventHandle::mouseDoubleClickEvent(QMouseEvent *ev)
{
	Abstract2dEventHandle::mouseDoubleClickEvent(ev);
}

void Transform2dPatternEventHandle::mouseMoveEvent(QMouseEvent *ev)
{
	Abstract2dEventHandle::mouseMoveEvent(ev);

	auto manager = m_viewer->getManager();
	if (manager == nullptr)
		return;

	if (panelLevelTransform_MouseMove(ev))
		return;
	if (curveLevelTransform_MouseMove(ev))
		return;
	if (pointLevelTransform_MouseMove(ev))
		return;
}

void Transform2dPatternEventHandle::wheelEvent(QWheelEvent *ev)
{
	Abstract2dEventHandle::wheelEvent(ev);
}

void Transform2dPatternEventHandle::keyPressEvent(QKeyEvent *ev)
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

	for (size_t iPiece = 0; iPiece < manager->numClothPieces(); iPiece++)
	{
		auto piece = manager->clothPiece(iPiece);
		auto& panel = piece->panel();
		panel.select(0, op);
	} // end for iPiece
}

void Transform2dPatternEventHandle::keyReleaseEvent(QKeyEvent *ev)
{
	Abstract2dEventHandle::keyReleaseEvent(ev);
}

bool Transform2dPatternEventHandle::panelLevelTransform_MouseMove(QMouseEvent* ev)
{
	auto manager = m_viewer->getManager();
	if (manager == nullptr)
		return false;
	ldp::Float3 lp3(m_viewer->lastMousePos().x(), m_viewer->height() - 1 - m_viewer->lastMousePos().y(), 1);
	ldp::Float3 p3(ev->x(), m_viewer->height() - 1 - ev->y(), 1);
	lp3 = m_viewer->camera().getWorldCoords(lp3);
	p3 = m_viewer->camera().getWorldCoords(p3);
	ldp::Float2 lp(lp3[0], lp3[1]);
	ldp::Float2 p(p3[0], p3[1]);

	bool changed = false;
	// left button, translate ------------------------------------------------------
	if (ev->buttons() == Qt::LeftButton && !(ev->modifiers() & Qt::ALT))
	{
		for (size_t iPiece = 0; iPiece < manager->numClothPieces(); iPiece++)
		{
			auto piece = manager->clothPiece(iPiece);
			auto& panel = piece->panel();
			if (pickInfo().renderId == panel.getId() || panel.isSelected())
			{
				panel.translate(p - lp);
				panel.updateBound();
				m_transformed = true;
				changed = true;
				// reverse move 3D piece if wanted
				if (ev->modifiers() & Qt::SHIFT)
				{
					auto R3 = piece->transformInfo().transform().getRotationPart();
					auto dif = p - lp;
					piece->transformInfo().translate(R3 * ldp::Float3(-dif[0], -dif[1], 0));
				}
			}
		} // end for iPiece
	} // end if left button

	// right button, rotate -------------------------------------------------
	if (ev->buttons() == Qt::RightButton && !(ev->modifiers() & Qt::ALT))
	{
		ldp::Float2 ld = (lp - m_rotateCenter).normalize();
		ldp::Float2 d = (p - m_rotateCenter).normalize();
		float ltheta = atan2(ld[1], ld[0]);
		float theta = atan2(d[1], d[0]);
		float dr = theta - ltheta;
		ldp::Mat2f R;
		R(0, 0) = cos(dr);		R(0, 1) = -sin(dr);
		R(1, 0) = sin(dr);		R(1, 1) = cos(dr);
		for (size_t iPiece = 0; iPiece < manager->numClothPieces(); iPiece++)
		{
			auto piece = manager->clothPiece(iPiece);
			auto& panel = piece->panel();
			if (pickInfo().renderId == panel.getId() || panel.isSelected())
			{
				panel.rotateBy(R, m_rotateCenter);
				m_transformed = true;
				changed = true;
				// reverse move 3D piece if wanted
				if (ev->modifiers() & Qt::SHIFT)
				{
					auto R3 = piece->transformInfo().transform().getRotationPart();
					auto t3 = piece->transformInfo().transform().getTranslationPart();
					ldp::Float3 c(m_rotateCenter[0], m_rotateCenter[1], 0);
					ldp::Mat3f R2 = ldp::Mat3f().eye();
					R2(0, 0) = R(0, 0);		R2(0, 1) = R(0, 1);
					R2(1, 0) = R(1, 0);		R2(1, 1) = R(1, 1);
					piece->transformInfo().rotate(R3*R2.inv()*R3.inv(), t3 + R3*c - R3*R2*c);
					piece->transformInfo().translate(R3*R2*c - R3*c);
				}
			}
		} // end for iPiece
	} // end if right button

	// right button + ALT, scale -------------------------------------------------
	if (ev->buttons() == Qt::RightButton && (ev->modifiers() & Qt::ALT))
	{
		float ld = (lp - m_rotateCenter).length();
		float d = (p - m_rotateCenter).length();
		float s = d / ld;
		for (size_t iPiece = 0; iPiece < manager->numClothPieces(); iPiece++)
		{
			auto piece = manager->clothPiece(iPiece);
			auto& panel = piece->panel();
			if (pickInfo().renderId == panel.getId() || panel.isSelected())
			{
				panel.scaleBy(s, m_rotateCenter);
				m_transformed = true;
				changed = true;
				// reverse move 3D piece if wanted
				if (ev->modifiers() & Qt::SHIFT)
				{
					auto R3 = piece->transformInfo().transform().getRotationPart();
					auto t3 = piece->transformInfo().transform().getTranslationPart();
					ldp::Float3 c(m_rotateCenter[0], m_rotateCenter[1], 0);
					piece->transformInfo().scale(1.f / s, t3 + R3*c - R3*s*c);
					piece->transformInfo().translate(R3*s*c - R3*c);
				}
			}
		} // end for iPiece
	} // end if right button + ALT
	return changed;
} 

bool Transform2dPatternEventHandle::curveLevelTransform_MouseMove(QMouseEvent* ev)
{
	auto manager = m_viewer->getManager();
	if (manager == nullptr)
		return false;
	ldp::Float3 lp3(m_viewer->lastMousePos().x(), m_viewer->height() - 1 - m_viewer->lastMousePos().y(), 1);
	ldp::Float3 p3(ev->x(), m_viewer->height() - 1 - ev->y(), 1);
	lp3 = m_viewer->camera().getWorldCoords(lp3);
	p3 = m_viewer->camera().getWorldCoords(p3);
	ldp::Float2 lp(lp3[0], lp3[1]);
	ldp::Float2 p(p3[0], p3[1]);

	bool changed = false;
	// left button, translate ------------------------------------------------------
	if (ev->buttons() == Qt::LeftButton && !(ev->modifiers() & Qt::ALT))
	{
		for (size_t iPiece = 0; iPiece < manager->numClothPieces(); iPiece++)
		{
			ldp::ClothPiece* piece = manager->clothPiece(iPiece);
			ldp::PanelPolygon& panel = piece->panel();
			ldp::ShapeGroup* poly = panel.outerPoly().get();
			if (poly == nullptr)
				continue;
			if (poly->empty())
				continue;

			// poly
			for (size_t iShape = 0, iLast = poly->size()-1; iShape < poly->size(); iLast = iShape++)
			{
				ldp::AbstractShape* shape = (*poly)[iShape].get();
				// shape
				if (shape->isSelected() || pickInfo().renderId == shape->getId())
				{
					shape->translate(p - lp);
					// next shape
					ldp::AbstractShape* nextShape = (*poly)[(iShape + 1) % (*poly).size()].get();
					if (!nextShape->isSelected() && pickInfo().renderId != nextShape->getId())
						nextShape->translateKeyPoint(0, p - lp);
					// last shape
					ldp::AbstractShape* lastShape = (*poly)[iLast].get();
					if (!lastShape->isSelected() && pickInfo().renderId != lastShape->getId())
						lastShape->translateKeyPoint(lastShape->numKeyPoints() - 1, p - lp);
					changed = true;
					m_transformed = true;
				}
			} // end for iShape

			// darts
			for (size_t iDart = 0; iDart < panel.darts().size(); iDart++)
			{
				ldp::ShapeGroup* poly = panel.darts()[iDart].get();
				for (size_t iShape = 0, iLast = poly->size() - 1; iShape < poly->size(); iLast = iShape++)
				{
					ldp::AbstractShape* shape = (*poly)[iShape].get();
					// shape
					if (shape->isSelected() || pickInfo().renderId == shape->getId())
					{
						shape->translate(p - lp);
						// next shape
						ldp::AbstractShape* nextShape = (*poly)[(iShape + 1) % (*poly).size()].get();
						if (!nextShape->isSelected() && pickInfo().renderId != nextShape->getId())
							nextShape->translateKeyPoint(0, p - lp);
						// last shape
						ldp::AbstractShape* lastShape = (*poly)[iLast].get();
						if (!lastShape->isSelected() && pickInfo().renderId != lastShape->getId())
							lastShape->translateKeyPoint(lastShape->numKeyPoints() - 1, p - lp);
						changed = true;
						m_transformed = true;
					}
				} // end for iShape
			} // end for iDart

			// inner lines
			for (size_t iLine = 0; iLine < panel.innerLines().size(); iLine++)
			{
				ldp::ShapeGroup* poly = panel.innerLines()[iLine].get();
				for (size_t iShape = 0, iLast = poly->size() - 1; iShape < poly->size(); iLast = iShape++)
				{
					ldp::AbstractShape* shape = (*poly)[iShape].get();
					// shape
					if (shape->isSelected() || pickInfo().renderId == shape->getId())
					{
						shape->translate(p - lp);
						// next shape
						if (iShape + 1 < (*poly).size())
						{
							ldp::AbstractShape* nextShape = (*poly)[iShape + 1].get();
							if (!nextShape->isSelected() && pickInfo().renderId != nextShape->getId())
								nextShape->translateKeyPoint(0, p - lp);
						}
						// last shape
						if (iShape > 0)
						{
							ldp::AbstractShape* lastShape = (*poly)[iLast].get();
							if (!lastShape->isSelected() && pickInfo().renderId != lastShape->getId())
								lastShape->translateKeyPoint(lastShape->numKeyPoints() - 1, p - lp);
						}
						changed = true;
						m_transformed = true;
					}
				} // end for iShape
			} // end for iLines
		} // end for iPiece
	} // end if left button
	return changed;
}

bool Transform2dPatternEventHandle::pointLevelTransform_MouseMove(QMouseEvent* ev)
{
	auto manager = m_viewer->getManager();
	if (manager == nullptr)
		return false;
	ldp::Float3 lp3(m_viewer->lastMousePos().x(), m_viewer->height() - 1 - m_viewer->lastMousePos().y(), 1);
	ldp::Float3 p3(ev->x(), m_viewer->height() - 1 - ev->y(), 1);
	lp3 = m_viewer->camera().getWorldCoords(lp3);
	p3 = m_viewer->camera().getWorldCoords(p3);
	ldp::Float2 lp(lp3[0], lp3[1]);
	ldp::Float2 p(p3[0], p3[1]);

	bool changed = false;
	// left button, translate ------------------------------------------------------
	if (ev->buttons() == Qt::LeftButton && !(ev->modifiers() & Qt::ALT))
	{
		for (size_t iPiece = 0; iPiece < manager->numClothPieces(); iPiece++)
		{
			ldp::ClothPiece* piece = manager->clothPiece(iPiece);
			ldp::PanelPolygon& panel = piece->panel();
			ldp::ShapeGroup* poly = panel.outerPoly().get();
			if (poly == nullptr)
				continue;
			if (poly->empty())
				continue;

			// poly
			for (size_t iShape = 0, iLast = poly->size() - 1; iShape < poly->size(); iLast = iShape++)
			{
				ldp::AbstractShape* shape = (*poly)[iShape].get();
				for (int k = 0; k < shape->numKeyPoints(); k++)
				{
					if (shape->getKeyPoint(k).isSelected() || pickInfo().renderId == shape->getKeyPoint(k).getId())
					{
						shape->translateKeyPoint(k, p - lp);
						ldp::AbstractShape* nextShape = (*poly)[(iShape + 1) % (*poly).size()].get();
						ldp::AbstractShape* lastShape = (*poly)[iLast].get();
						if (k == 0 && !lastShape->isSelected() && pickInfo().renderId != lastShape->getId())
							lastShape->translateKeyPoint(lastShape->numKeyPoints() - 1, p - lp);
						if (k == shape->numKeyPoints()-1 && !nextShape->isSelected() && 
							pickInfo().renderId != nextShape->getId())
							nextShape->translateKeyPoint(0, p - lp);
						changed = true;
						m_transformed = true;
					}
				} // end for k
			} // end for iShape

			// darts
			for (size_t iDart = 0; iDart < panel.darts().size(); iDart++)
			{
				ldp::ShapeGroup* poly = panel.darts()[iDart].get();
				for (size_t iShape = 0, iLast = poly->size() - 1; iShape < poly->size(); iLast = iShape++)
				{
					ldp::AbstractShape* shape = (*poly)[iShape].get();
					for (int k = 0; k < shape->numKeyPoints(); k++)
					{
						if (shape->getKeyPoint(k).isSelected() || pickInfo().renderId == shape->getKeyPoint(k).getId())
						{
							shape->translateKeyPoint(k, p - lp);
							ldp::AbstractShape* nextShape = (*poly)[(iShape + 1) % (*poly).size()].get();
							ldp::AbstractShape* lastShape = (*poly)[iLast].get();
							if (k == 0 && !lastShape->isSelected() && pickInfo().renderId != lastShape->getId())
								lastShape->translateKeyPoint(lastShape->numKeyPoints() - 1, p - lp);
							if (k == shape->numKeyPoints() - 1 && !nextShape->isSelected() &&
								pickInfo().renderId != nextShape->getId())
								nextShape->translateKeyPoint(0, p - lp);
							changed = true;
							m_transformed = true;
						}
					} // end for k
				} // end for iShape
			} // end for iDart

			// inner lines
			for (size_t iLine = 0; iLine < panel.innerLines().size(); iLine++)
			{
				ldp::ShapeGroup* poly = panel.innerLines()[iLine].get();
				for (size_t iShape = 0, iLast = poly->size() - 1; iShape < poly->size(); iLast = iShape++)
				{
					ldp::AbstractShape* shape = (*poly)[iShape].get();
					for (int k = 0; k < shape->numKeyPoints(); k++)
					{
						if (shape->getKeyPoint(k).isSelected() || pickInfo().renderId == shape->getKeyPoint(k).getId())
						{
							shape->translateKeyPoint(k, p - lp);
							// next shape
							if (iShape + 1 < (*poly).size() && k == shape->numKeyPoints() - 1)
							{
								ldp::AbstractShape* nextShape = (*poly)[iShape + 1].get();
								if (!nextShape->isSelected() && pickInfo().renderId != nextShape->getId())
									nextShape->translateKeyPoint(0, p - lp);
							}
							// last shape
							if (iShape > 0 && k == 0)
							{
								ldp::AbstractShape* lastShape = (*poly)[iLast].get();
								if (!lastShape->isSelected() && pickInfo().renderId != lastShape->getId())
									lastShape->translateKeyPoint(lastShape->numKeyPoints() - 1, p - lp);
							}
							changed = true;
							m_transformed = true;
						}
					} // end for k
				} // end for iShape
			} // end for iLines
		} // end for iPiece
	} // end if left button
	return changed;
}

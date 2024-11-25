/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#include "decoration.h"
#include "decoratedwindow.h"
#include "decoration_p.h"
#include "decorationbutton.h"
#include "decorationsettings.h"
#include "private/decoratedwindowprivate.h"
#include "private/decorationbridge.h"

#include <QCoreApplication>
#include <QHoverEvent>

#include <cmath>

namespace KDecoration3
{
namespace
{
DecorationBridge *findBridge(const QVariantList &args)
{
    for (const auto &arg : args) {
        if (auto bridge = arg.toMap().value(QStringLiteral("bridge")).value<DecorationBridge *>()) {
            return bridge;
        }
    }
    Q_UNREACHABLE();
}
}

Decoration::Private::Private(Decoration *deco, const QVariantList &args)
    : sectionUnderMouse(Qt::NoSection)
    , bridge(findBridge(args))
    , client(std::shared_ptr<DecoratedWindow>(new DecoratedWindow(deco, bridge)))
    , opaque(false)
    , q(deco)
{
}

void Decoration::Private::setSectionUnderMouse(Qt::WindowFrameSection section)
{
    if (sectionUnderMouse == section) {
        return;
    }
    sectionUnderMouse = section;
    Q_EMIT q->sectionUnderMouseChanged(sectionUnderMouse);
}

void Decoration::Private::updateSectionUnderMouse(const QPoint &mousePosition)
{
    if (titleBar.toRect().contains(mousePosition)) {
        setSectionUnderMouse(Qt::TitleBarArea);
        return;
    }
    const QSizeF size = q->size();
    const int corner = 2 * settings->largeSpacing();
    const bool left = mousePosition.x() < borders.left();
    const bool top = mousePosition.y() < borders.top();
    const bool bottom = mousePosition.y() >= size.height() - borders.bottom();
    const bool right = mousePosition.x() >= size.width() - borders.right();
    if (left) {
        if (top && mousePosition.y() < titleBar.top() + corner) {
            setSectionUnderMouse(Qt::TopLeftSection);
        } else if (mousePosition.y() >= size.height() - borders.bottom() - corner && mousePosition.y() >= titleBar.bottom()) {
            setSectionUnderMouse(Qt::BottomLeftSection);
        } else {
            setSectionUnderMouse(Qt::LeftSection);
        }
        return;
    }
    if (right) {
        if (top && mousePosition.y() < titleBar.top() + corner) {
            setSectionUnderMouse(Qt::TopRightSection);
        } else if (mousePosition.y() >= size.height() - borders.bottom() - corner && mousePosition.y() >= titleBar.bottom()) {
            setSectionUnderMouse(Qt::BottomRightSection);
        } else {
            setSectionUnderMouse(Qt::RightSection);
        }
        return;
    }
    if (bottom) {
        if (mousePosition.y() >= titleBar.bottom()) {
            if (mousePosition.x() < borders.left() + corner) {
                setSectionUnderMouse(Qt::BottomLeftSection);
            } else if (mousePosition.x() >= size.width() - borders.right() - corner) {
                setSectionUnderMouse(Qt::BottomRightSection);
            } else {
                setSectionUnderMouse(Qt::BottomSection);
            }
        } else {
            setSectionUnderMouse(Qt::TitleBarArea);
        }
        return;
    }
    if (top) {
        if (mousePosition.y() < titleBar.top()) {
            if (mousePosition.x() < borders.left() + corner) {
                setSectionUnderMouse(Qt::TopLeftSection);
            } else if (mousePosition.x() >= size.width() - borders.right() - corner) {
                setSectionUnderMouse(Qt::TopRightSection);
            } else {
                setSectionUnderMouse(Qt::TopSection);
            }
        } else {
            setSectionUnderMouse(Qt::TitleBarArea);
        }
        return;
    }
    setSectionUnderMouse(Qt::NoSection);
}

void Decoration::Private::addButton(DecorationButton *button)
{
    Q_ASSERT(!buttons.contains(button));
    buttons << button;
    QObject::connect(button, &QObject::destroyed, q, [this](QObject *o) {
        auto it = buttons.begin();
        while (it != buttons.end()) {
            if (*it == static_cast<DecorationButton *>(o)) {
                it = buttons.erase(it);
            } else {
                it++;
            }
        }
    });
}

Decoration::Decoration(QObject *parent, const QVariantList &args)
    : QObject(parent)
    , d(new Private(this, args))
{
    connect(this, &Decoration::bordersChanged, this, [this] {
        update();
    });
}

Decoration::~Decoration() = default;

DecoratedWindow *Decoration::window() const
{
    return d->client.get();
}

void Decoration::requestClose()
{
    d->client->d->requestClose();
}

void Decoration::requestContextHelp()
{
    d->client->d->requestContextHelp();
}

void Decoration::requestMinimize()
{
    d->client->d->requestMinimize();
}

void Decoration::requestToggleOnAllDesktops()
{
    d->client->d->requestToggleOnAllDesktops();
}

void Decoration::requestToggleShade()
{
    d->client->d->requestToggleShade();
}

void Decoration::requestToggleKeepAbove()
{
    d->client->d->requestToggleKeepAbove();
}

void Decoration::requestToggleKeepBelow()
{
    d->client->d->requestToggleKeepBelow();
}

#if KDECORATIONS3_ENABLE_DEPRECATED_SINCE(5, 21)
void Decoration::requestShowWindowMenu()
{
    requestShowWindowMenu(QRect());
}
#endif

void Decoration::requestShowWindowMenu(const QRect &rect)
{
    d->client->d->requestShowWindowMenu(rect);
}

void Decoration::requestShowToolTip(const QString &text)
{
    d->client->d->requestShowToolTip(text);
}

void Decoration::requestHideToolTip()
{
    d->client->d->requestHideToolTip();
}

void Decoration::requestToggleMaximization(Qt::MouseButtons buttons)
{
    d->client->d->requestToggleMaximization(buttons);
}

void Decoration::showApplicationMenu(int actionId)
{
    const auto it = std::find_if(d->buttons.constBegin(), d->buttons.constEnd(), [](DecorationButton *button) {
        return button->type() == DecorationButtonType::ApplicationMenu;
    });
    if (it != d->buttons.constEnd()) {
        requestShowApplicationMenu((*it)->geometry().toRect(), actionId);
    }
}

void Decoration::requestShowApplicationMenu(const QRect &rect, int actionId)
{
    d->client->d->requestShowApplicationMenu(rect, actionId);
}

void Decoration::setBlurRegion(const QRegion &region)
{
    if (d->blurRegion != region) {
        d->blurRegion = region;
        Q_EMIT blurRegionChanged();
    }
}

void Decoration::setBorders(const QMarginsF &borders)
{
    if (d->borders != borders) {
        d->borders = borders;
        Q_EMIT bordersChanged();
    }
}

void Decoration::setResizeOnlyBorders(const QMarginsF &borders)
{
    if (d->resizeOnlyBorders != borders) {
        d->resizeOnlyBorders = borders;
        Q_EMIT resizeOnlyBordersChanged();
    }
}

void Decoration::setTitleBar(const QRectF &rect)
{
    if (d->titleBar != rect) {
        d->titleBar = rect;
        Q_EMIT titleBarChanged();
    }
}

void Decoration::setOpaque(bool opaque)
{
    if (d->opaque != opaque) {
        d->opaque = opaque;
        Q_EMIT opaqueChanged(opaque);
    }
}

void Decoration::setShadow(const std::shared_ptr<DecorationShadow> &shadow)
{
    if (d->shadow != shadow) {
        d->shadow = shadow;
        Q_EMIT shadowChanged(shadow);
    }
}

QRegion Decoration::blurRegion() const
{
    return d->blurRegion;
}

QMarginsF Decoration::borders() const
{
    return d->borders;
}

QMarginsF Decoration::resizeOnlyBorders() const
{
    return d->resizeOnlyBorders;
}

QRectF Decoration::titleBar() const
{
    return d->titleBar;
}

Qt::WindowFrameSection Decoration::sectionUnderMouse() const
{
    return d->sectionUnderMouse;
}

std::shared_ptr<DecorationShadow> Decoration::shadow() const
{
    return d->shadow;
}

bool Decoration::isOpaque() const
{
    return d->opaque;
}

qreal Decoration::borderLeft() const
{
    return d->borders.left();
}

qreal Decoration::resizeOnlyBorderLeft() const
{
    return d->resizeOnlyBorders.left();
}

qreal Decoration::borderRight() const
{
    return d->borders.right();
}

qreal Decoration::resizeOnlyBorderRight() const
{
    return d->resizeOnlyBorders.right();
}

qreal Decoration::borderTop() const
{
    return d->borders.top();
}

qreal Decoration::resizeOnlyBorderTop() const
{
    return d->resizeOnlyBorders.top();
}

qreal Decoration::borderBottom() const
{
    return d->borders.bottom();
}

qreal Decoration::resizeOnlyBorderBottom() const
{
    return d->resizeOnlyBorders.bottom();
}

QSizeF Decoration::size() const
{
    const QMarginsF &b = d->borders;
    return QSizeF(d->client->width() + b.left() + b.right(), (d->client->isShaded() ? 0 : d->client->height()) + b.top() + b.bottom());
}

QRectF Decoration::rect() const
{
    return QRectF(QPointF(0, 0), size());
}

bool Decoration::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
        hoverEnterEvent(static_cast<QHoverEvent *>(event));
        return true;
    case QEvent::HoverLeave:
        hoverLeaveEvent(static_cast<QHoverEvent *>(event));
        return true;
    case QEvent::HoverMove:
        hoverMoveEvent(static_cast<QHoverEvent *>(event));
        return true;
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::MouseMove:
        mouseMoveEvent(static_cast<QMouseEvent *>(event));
        return true;
    case QEvent::Wheel:
        wheelEvent(static_cast<QWheelEvent *>(event));
        return true;
    default:
        return QObject::event(event);
    }
}

void Decoration::hoverEnterEvent(QHoverEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        QCoreApplication::instance()->sendEvent(button, event);
    }
    auto flooredPos = QPoint(std::floor(event->position().x()), std::floor(event->position().y()));
    d->updateSectionUnderMouse(flooredPos);
}

void Decoration::hoverLeaveEvent(QHoverEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        QCoreApplication::instance()->sendEvent(button, event);
    }
    d->setSectionUnderMouse(Qt::NoSection);
}

void Decoration::hoverMoveEvent(QHoverEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (!button->isEnabled() || !button->isVisible()) {
            continue;
        }
        const bool hovered = button->isHovered();
        const bool contains = button->contains(event->position());
        if (!hovered && contains) {
            QHoverEvent e(QEvent::HoverEnter, event->position(), event->oldPosF(), event->modifiers());
            QCoreApplication::instance()->sendEvent(button, &e);
        } else if (hovered && !contains) {
            QHoverEvent e(QEvent::HoverLeave, event->position(), event->oldPosF(), event->modifiers());
            QCoreApplication::instance()->sendEvent(button, &e);
        } else if (hovered && contains) {
            QCoreApplication::instance()->sendEvent(button, event);
        }
    }
    auto flooredPos = QPoint(std::floor(event->position().x()), std::floor(event->position().y()));
    d->updateSectionUnderMouse(flooredPos);
}

void Decoration::mouseMoveEvent(QMouseEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (button->isPressed()) {
            QCoreApplication::instance()->sendEvent(button, event);
            return;
        }
    }
    // not handled, take care ourselves
}

void Decoration::mousePressEvent(QMouseEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (button->isHovered()) {
            if (button->acceptedButtons().testFlag(event->button())) {
                QCoreApplication::instance()->sendEvent(button, event);
            }
            event->setAccepted(true);
            return;
        }
    }
}

void Decoration::mouseReleaseEvent(QMouseEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (button->isPressed() && button->acceptedButtons().testFlag(event->button())) {
            QCoreApplication::instance()->sendEvent(button, event);
            return;
        }
    }
    // not handled, take care ourselves
    d->updateSectionUnderMouse(event->pos());
}

void Decoration::wheelEvent(QWheelEvent *event)
{
    for (DecorationButton *button : d->buttons) {
        if (button->contains(event->position())) {
            QCoreApplication::instance()->sendEvent(button, event);
            event->setAccepted(true);
        }
    }
}

void Decoration::update(const QRectF &r)
{
    Q_EMIT damaged(r.isNull() ? rect().toAlignedRect() : r.toAlignedRect());
}

void Decoration::update()
{
    update(QRect());
}

void Decoration::setSettings(const std::shared_ptr<DecorationSettings> &settings)
{
    d->settings = settings;
}

std::shared_ptr<DecorationSettings> Decoration::settings() const
{
    return d->settings;
}

} // namespace

#include "moc_decoration.cpp"

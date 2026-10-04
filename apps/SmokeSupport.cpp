#if defined(_WIN32) && !defined(VOELUND_VIEWPORT_QT_OPENGL)
#include <Windows.h>
#include <gl/GL.h>
#endif
#include "SmokeSupport.hpp"
#include <Font_FontMgr.hxx>
#include <QAbstractButton>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QScreen>
#include <QSysInfo>
#include <Standard_Version.hxx>
#include <cstdio>

namespace voelund::apps {
namespace {
QJsonObject diagnostics(viewport::CadViewport& viewport) {
    const auto [width, height] = viewport.renderingPixelSize();
    QJsonObject result{{"qt", qVersion()},
                       {"occt", OCC_VERSION_COMPLETE},
                       {"qpa", QGuiApplication::platformName()},
                       {"os", QSysInfo::prettyProductName()},
                       {"dpr", viewport.devicePixelRatioF()},
                       {"dpi", viewport.screen()->logicalDotsPerInch()},
                       {"width", width},
                       {"height", height}};
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    result["backend"] = "qt-opengl";
    viewport.makeCurrent();
    auto* context = viewport.context();
    if (context == nullptr || !context->isValid() || QOpenGLContext::currentContext() != context)
        return result;
    const auto glString = [context](GLenum name) {
        const auto* value = context->functions()->glGetString(name);
        return value == nullptr ? QString()
                                : QString::fromLatin1(reinterpret_cast<const char*>(value));
    };
#else
    result["backend"] = "native";
    if (wglGetCurrentContext() == nullptr ||
        WindowFromDC(wglGetCurrentDC()) != reinterpret_cast<HWND>(viewport.winId()))
        return result;
    const auto glString = [](GLenum name) {
        const auto* value = glGetString(name);
        return value == nullptr ? QString()
                                : QString::fromLatin1(reinterpret_cast<const char*>(value));
    };
#endif
    result["gl"] = glString(GL_VERSION);
    result["vendor"] = glString(GL_VENDOR);
    result["renderer"] = glString(GL_RENDERER);
    Font_FontAspect fontAspect = Font_FA_Regular;
    result["courierFontAvailable"] =
        !Font_FontMgr::GetInstance()->FindFont("Courier", fontAspect).IsNull();
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    viewport.doneCurrent();
#endif
    return result;
}

QImage renderingImage(viewport::CadViewport& viewport) {
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    return viewport.grabFramebuffer();
#else
    if (wglGetCurrentContext() == nullptr ||
        WindowFromDC(wglGetCurrentDC()) != reinterpret_cast<HWND>(viewport.winId()))
        return {};
    const auto [width, height] = viewport.renderingPixelSize();
    QImage image(width, height, QImage::Format_RGBA8888);
    GLint previousBuffer = 0;
    GLint previousAlignment = 0;
    glGetIntegerv(GL_READ_BUFFER, &previousBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
    const bool success = glGetError() == GL_NO_ERROR;
    glReadBuffer(static_cast<GLenum>(previousBuffer));
    glPixelStorei(GL_PACK_ALIGNMENT, previousAlignment);
    return success ? image.flipped(Qt::Vertical) : QImage{};
#endif
}
} // namespace

void enableSmokeLogging() {
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& message) {
        const QByteArray bytes = message.toUtf8();
        std::fprintf(stderr, "%s\n", bytes.constData());
        std::fflush(stderr);
    });
}

bool verifyDesktopSmoke(viewport::CadViewport& viewport) {
    const auto [width, height] = viewport.renderingPixelSize();
    if (!viewport.isVisible() || !viewport.isRenderingReady() || width <= 0 || height <= 0) {
        qInfo() << "SMOKE_REJECTED uninitialized";
        return false;
    }
    const QString actualQpa = QGuiApplication::platformName();
    QString requestedQpa = qEnvironmentVariable("QT_QPA_PLATFORM").section(':', 0, 0);
#if defined(_WIN32)
    if (requestedQpa.isEmpty())
        requestedQpa = QStringLiteral("windows");
#elif defined(__FreeBSD__) || defined(__linux__)
    if (requestedQpa.isEmpty())
        requestedQpa = QStringLiteral("wayland");
#endif
    if ((!requestedQpa.isEmpty() && !actualQpa.startsWith(requestedQpa)) ||
        actualQpa == QStringLiteral("offscreen") || actualQpa == QStringLiteral("minimal")) {
        qCritical() << "SMOKE_REJECTED qpa";
        return false;
    }
    const auto info = diagnostics(viewport);
    qInfo().noquote() << "desktop-smoke" << QJsonDocument(info).toJson(QJsonDocument::Compact);
    return !info.value("gl").toString().isEmpty() && !info.value("renderer").toString().isEmpty();
}

bool captureDesktopSmoke(QWidget& window, viewport::CadViewport& viewport) {
    const QImage frame = renderingImage(viewport);
    if (frame.isNull())
        return false;
    // Necessary only: nonuniform pixels do not prove model content. Review images too.
    bool varied = false;
    for (int y = 0; y < frame.height(); y += 8)
        for (int x = 0; x < frame.width(); x += 8)
            varied |= frame.pixel(x, y) != frame.pixel(0, 0);
    if (!varied)
        return false;
    const QString directory = qEnvironmentVariable("VOELUND_SMOKE_ARTIFACT_DIR");
    if (directory.isEmpty())
        return true;
    if (!QDir().mkpath(directory))
        return false;
    const QString stem =
        QDir(directory).filePath(QFileInfo(QCoreApplication::applicationFilePath()).baseName());
    if (!frame.save(stem + "-framebuffer.png") || !window.grab().save(stem + "-widgets.png"))
        return false;
    auto info = diagnostics(viewport);
    info["capture"] = "QWidget::grab and separate framebuffer; no compositor proof";
    QFile output(stem + ".json");
    const auto data = QJsonDocument(info).toJson();
    return output.open(QIODevice::WriteOnly) && output.write(data) == data.size();
}

bool clickSmokeButton(QAbstractButton& button) {
    if (!button.isVisible() || !button.isEnabled())
        return false;
    const QPointF point(button.rect().center());
    const QPointF global(button.mapToGlobal(point.toPoint()));
    QMouseEvent press(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&button, &press);
    QCoreApplication::sendEvent(&button, &release);
    return true;
}

bool enterSmokeValue(QDoubleSpinBox& input, const QString& text) {
    if (!input.isVisible() || !input.isEnabled())
        return false;
    input.setFocus();
    input.selectAll();
    QKeyEvent textEvent(QEvent::KeyPress, 0, Qt::NoModifier, text);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(&input, &textEvent);
    QCoreApplication::sendEvent(&input, &enter);
    return true;
}
} // namespace voelund::apps

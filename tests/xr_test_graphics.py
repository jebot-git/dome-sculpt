"""Graphics requirements for native OpenXR application tests."""

import os


def hardware_environment():
    """Keep the caller's device selection and clear the software oracle override."""
    environment = os.environ.copy()
    environment.pop("LIBGL_ALWAYS_SOFTWARE", None)
    return environment


def hardware_renderer(report, expected=None):
    """Check the renderer reported by the OpenXR runtime across GPU vendors."""
    renderer = report.get("gl_renderer", "").strip().casefold()
    software = (
        "llvmpipe", "softpipe", "swrast", "software rasterizer",
        "software renderer", "swiftshader", "gdi generic",
        "microsoft basic render driver",
    )
    expected = expected or os.environ.get("DOME_TEST_GL_RENDERER")
    return bool(renderer) and not any(name in renderer for name in software) and (
        not expected or expected.casefold() in renderer
    )

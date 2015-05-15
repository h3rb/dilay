#include "cache.hpp"
#include "sculpt-brush.hpp"
#include "tools.hpp"
#include "view/cursor.hpp"
#include "view/double-slider.hpp"
#include "view/properties.hpp"
#include "view/tool-tip.hpp"
#include "view/util.hpp"

struct ToolSculptFlatten::Impl {
  ToolSculptFlatten* self;

  Impl (ToolSculptFlatten* s) : self (s) {}

  void runSetupBrush (SculptBrush& brush) {
    auto& params = brush.parameters <SBFlattenParameters> ();

    params.intensity (this->self->cache ().get <float> ("intensity", 0.5f));
  }

  void runSetupCursor (ViewCursor&) {}

  void runSetupProperties (ViewPropertiesPart& properties) {
    auto& params = this->self->brush ().parameters <SBFlattenParameters> ();

    ViewDoubleSlider& intensityEdit = ViewUtil::slider ( 0.1f, params.intensity ()
                                                       , 1.0f, 0.1f );
    ViewUtil::connect (intensityEdit, [this,&params] (float i) {
      params.intensity (i);
      this->self->cache ().set ("intensity", i);
    });
    properties.addStacked (QObject::tr ("Intensity"), intensityEdit);
  }

  void runSetupToolTip (ViewToolTip& toolTip) {
    toolTip.add (ViewToolTip::MouseEvent::Left, QObject::tr ("Drag to flatten"));
  }

  void runSculptMouseMoveEvent (const QMouseEvent& e) {
    this->self->carvelikeStroke (e);
  }

  bool runSculptMousePressEvent (const QMouseEvent& e) {
    return this->self->carvelikeStroke (e);
  }
};

DELEGATE_TOOL_SCULPT (ToolSculptFlatten)

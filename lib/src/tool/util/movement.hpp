/* This file is part of Dilay
 * Copyright © 2015,2016 Alexander Bau
 * Use and redistribute under the terms of the GNU General Public License
 */
#ifndef DILAY_TOOL_MOVEMENT
#define DILAY_TOOL_MOVEMENT

#include <functional>
#include <glm/fwd.hpp>
#include "macro.hpp"

class Camera;
class ViewPointingEvent;
class ViewTwoColumnGrid;

enum class MovementFixedConstraint
{
  XAxis,
  YAxis,
  ZAxis,
  XYPlane,
  XZPlane,
  YZPlane,
  CameraPlane,
  PrimaryPlane
};

class ToolUtilMovement
{
public:
  // Constrained to fixed plane or axis
  DECLARE_BIG3 (ToolUtilMovement, const Camera&, MovementFixedConstraint)

  MovementFixedConstraint fixedConstraint () const;
  void                    fixedConstraint (MovementFixedConstraint);
  void                    addFixedProperties (ViewTwoColumnGrid&, const std::function<void()>&);

  // Constrained to free plane
  ToolUtilMovement (const Camera&, const glm::vec3&);

  const glm::vec3& freePlaneConstraint () const;
  void             freePlaneConstraint (const glm::vec3&);

  glm::vec3        delta () const;
  const glm::vec3& position () const;
  void             position (const glm::vec3&);
  bool             move (const ViewPointingEvent&, bool);
  void             resetPosition (const glm::vec3&);

private:
  IMPLEMENTATION
};

#endif

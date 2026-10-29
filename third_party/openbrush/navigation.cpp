// SPDX-License-Identifier: Apache-2.0
#include "navigation.h"
namespace ob {
std::optional<Transform>
Navigation::update(const std::array<TrackedHand, 2> &hands, Transform current) {
  int next = 0;
  for (int i = 0; i < 2; ++i)
    if (hands[i].valid && hands[i].squeeze > .55f)
      next |= 1 << i;
  if (next == 3 &&
      length(hands[1].grip.position - hands[0].grip.position) < .04f)
    next = 1;
  Transform pose;
  if (next == 3)
    pose.position = (hands[0].grip.position + hands[1].grip.position) * .5f;
  else if (next)
    pose = hands[next == 1 ? 0 : 1].grip;
  if (next != mask) {
    mask = next;
    base = current;
    start = pose;
    if (next == 3)
      startVector = hands[1].grip.position - hands[0].grip.position;
    return {};
  }
  if (!mask)
    return {};
  if (mask == 3) {
    auto vector = hands[1].grip.position - hands[0].grip.position;
    pose.orientation = between(startVector, vector);
    pose.scale = std::clamp(length(vector) / length(startVector),
                            .05f / base.scale, 20.f / base.scale);
  }
  auto result = compose(compose(pose, inverse(start)), base);
  if (!finite(result))
    return {};
  result.scale = std::clamp(result.scale, .05f, 20.f);
  return result;
}
} // namespace ob

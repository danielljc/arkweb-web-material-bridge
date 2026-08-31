#ifndef ARKWEB_WEB_MATERIAL_TEST_FIXTURES_H_
#define ARKWEB_WEB_MATERIAL_TEST_FIXTURES_H_

#include "web_material/collector.h"

#include <cstdint>
#include <string>
#include <utility>

namespace arkweb::material::test {

inline DocumentContext MakeContext(DocumentId id = 7U)
{
    DocumentContext context;
    context.document_id = id;
    context.origin = "https://example.test";
    context.trust_level = TrustLevel::kApplication;
    context.color_scheme = ColorScheme::kLight;
    context.contrast = ContrastPreference::kNormal;
    context.power_preference = PowerPreference::kDefault;
    context.device_scale_factor = 2.0F;
    context.viewport = {0.0F, 0.0F, 1080.0F, 2400.0F};
    context.navigation_epoch = 1U;
    return context;
}

inline MaterialDescriptor MakeDescriptor(ElementId id = 101U,
    DocumentId document_id = 7U,
    MaterialRole role = MaterialRole::kNavigation)
{
    MaterialDescriptor descriptor;
    descriptor.element_id = id;
    descriptor.document_id = document_id;
    descriptor.role = role;
    descriptor.viewport_rect = {20.0F, 40.0F, 1040.0F, 128.0F};
    descriptor.corner_radii = CornerRadii::Uniform(32.0F);
    descriptor.local_to_viewport = Matrix3::Translation(20.0F, 40.0F);
    descriptor.opacity = 1.0F;
    descriptor.z_order = 10;
    descriptor.color_scheme = ColorScheme::kLight;
    descriptor.contrast = ContrastPreference::kNormal;
    descriptor.power_preference = PowerPreference::kDefault;
    descriptor.trust_level = TrustLevel::kApplication;
    descriptor.source_revision = 1U;
    descriptor.debug_name = "navigation";
    return descriptor;
}

inline WebElementSnapshot MakeElement(std::uint64_t node_id = 42U,
    std::string role = "navigation")
{
    WebElementSnapshot element;
    element.backend_node_id = node_id;
    element.dom_id = "element-" + std::to_string(node_id);
    element.material_attribute = std::move(role);
    element.local_bounds = {0.0F, 0.0F, 500.0F, 80.0F};
    element.corner_radii = CornerRadii::Uniform(20.0F);
    element.local_to_document = Matrix3::Translation(20.0F, 100.0F);
    element.opacity = 1.0F;
    element.z_order = 5;
    element.style_revision = 1U;
    element.geometry_revision = 1U;
    return element;
}

inline FrameSnapshot MakeFrame(DocumentId document_id = 7U)
{
    FrameSnapshot frame;
    frame.context = MakeContext(document_id);
    frame.layout_scroll_offset = {0.0F, 0.0F};
    frame.visual_viewport_offset = {0.0F, 0.0F};
    frame.page_scale = 1.0F;
    frame.frame_revision = 1U;
    return frame;
}

inline MaterialBatch MakeAddBatch(MaterialDescriptor descriptor,
    std::uint64_t batch_sequence = 1U,
    std::uint64_t update_sequence = 1U)
{
    MaterialBatch batch;
    batch.document_id = descriptor.document_id;
    batch.navigation_epoch = 1U;
    batch.batch_sequence = batch_sequence;
    MaterialUpdate update = MaterialUpdate::Add(std::move(descriptor));
    update.sequence = update_sequence;
    batch.updates.push_back(std::move(update));
    return batch;
}

inline ClipNode MakeClip(std::uint64_t id = 1U)
{
    ClipNode clip;
    clip.node_id = id;
    clip.rect = {0.0F, 0.0F, 600.0F, 400.0F};
    clip.radii = CornerRadii::Uniform(12.0F);
    clip.transform_to_viewport = Matrix3::Translation(10.0F, 20.0F);
    return clip;
}

}  // namespace arkweb::material::test

#endif  // ARKWEB_WEB_MATERIAL_TEST_FIXTURES_H_

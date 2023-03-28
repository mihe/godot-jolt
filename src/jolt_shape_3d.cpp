#include "jolt_shape_3d.hpp"

#include "jolt_collision_object_3d.hpp"
#include "jolt_override_user_data_shape.hpp"
#include "jolt_ray_shape.hpp"

JoltShape3D::~JoltShape3D() = default;

void JoltShape3D::add_owner(JoltCollisionObject3D* p_owner) {
	ref_counts_by_owner[p_owner]++;
}

void JoltShape3D::remove_owner(JoltCollisionObject3D* p_owner) {
	if (--ref_counts_by_owner[p_owner] <= 0) {
		ref_counts_by_owner.erase(p_owner);
	}
}

void JoltShape3D::remove_self(bool p_lock) {
	// `remove_owner` will be called when we `remove_shape`, so we need to copy the map since the
	// iterator would be invalidated from underneath us
	const auto ref_counts_by_owner_copy = ref_counts_by_owner;

	for (const auto& [owner, ref_count] : ref_counts_by_owner_copy) {
		owner->remove_shape(this, p_lock);
	}
}

JPH::ShapeRefC JoltShape3D::try_build(float p_extra_margin) {
	if (!is_valid()) {
		return {};
	}

	if (p_extra_margin > 0.0f) {
		return build(p_extra_margin);
	}

	if (jolt_ref == nullptr) {
		jolt_ref = build(0.0f);
	}

	return jolt_ref;
}

Vector3 JoltShape3D::get_center_of_mass() const {
	ERR_FAIL_NULL_D(jolt_ref);

	return to_godot(jolt_ref->GetCenterOfMass());
}

JPH::ShapeRefC JoltShape3D::with_scale(const JPH::Shape* p_shape, const Vector3& p_scale) {
	ERR_FAIL_NULL_D(p_shape);

	const JPH::ScaledShapeSettings shape_settings(p_shape, to_jolt(p_scale));
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to scale shape with scale '%v'. "
			"It returned the following error: '%s'.",
			p_scale,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

JPH::ShapeRefC JoltShape3D::with_basis_origin(
	const JPH::Shape* p_shape,
	const Basis& p_basis,
	const Vector3& p_origin
) {
	ERR_FAIL_NULL_D(p_shape);

	const JPH::RotatedTranslatedShapeSettings shape_settings(
		to_jolt(p_origin),
		to_jolt(p_basis),
		p_shape
	);

	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to offset shape with basis '%s' and origin '%v'. "
			"It returned the following error: '%s'.",
			p_basis,
			p_origin,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

JPH::ShapeRefC JoltShape3D::with_transform(
	const JPH::Shape* p_shape,
	const Transform3D& p_transform,
	const Vector3& p_scale
) {
	ERR_FAIL_NULL_D(p_shape);

	JPH::ShapeRefC shape = p_shape;

	if (p_scale != Vector3(1.0f, 1.0f, 1.0f)) {
		shape = with_scale(shape, p_scale);
	}

	if (p_transform != Transform3D()) {
		shape = with_basis_origin(shape, p_transform.basis, p_transform.origin);
	}

	return shape;
}

JPH::ShapeRefC JoltShape3D::with_center_of_mass_offset(
	const JPH::Shape* p_shape,
	const Vector3& p_offset
) {
	ERR_FAIL_NULL_D(p_shape);

	const JPH::OffsetCenterOfMassShapeSettings shape_settings(to_jolt(p_offset), p_shape);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to offset center of mass with offset '%v'. "
			"It returned the following error: '%s'.",
			p_offset,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

JPH::ShapeRefC JoltShape3D::with_center_of_mass(
	const JPH::Shape* p_shape,
	const Vector3& p_center_of_mass
) {
	ERR_FAIL_NULL_D(p_shape);

	const Vector3 center_of_mass_inner = to_godot(p_shape->GetCenterOfMass());
	const Vector3 center_of_mass_offset = p_center_of_mass - center_of_mass_inner;

	if (center_of_mass_offset == Vector3()) {
		return p_shape;
	}

	return with_center_of_mass_offset(p_shape, center_of_mass_offset);
}

JPH::ShapeRefC JoltShape3D::with_user_data(const JPH::Shape* p_shape, uint64_t p_user_data) {
	JoltOverrideUserDataShapeSettings shape_settings(p_shape);
	shape_settings.mUserData = (JPH::uint64)p_user_data;

	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to override user data. "
			"It returned the following error: '%s'.",
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

void JoltShape3D::shape_changed(bool p_lock) {
	for (const auto& [owner, ref_count] : ref_counts_by_owner) {
		owner->rebuild_shape(p_lock);
	}
}

Variant JoltWorldBoundaryShape3D::get_data() const {
	return plane;
}

void JoltWorldBoundaryShape3D::set_data(const Variant& p_data) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	ERR_FAIL_COND(p_data.get_type() != Variant::PLANE);

	initialize((Plane)p_data);
}

bool JoltWorldBoundaryShape3D::initialize(Plane p_plane) {
	if (p_plane == Plane()) {
		return false;
	}

	plane = p_plane;

	return true;
}

void JoltWorldBoundaryShape3D::clear() {
	jolt_ref = nullptr;
	plane = Plane();
}

JPH::ShapeRefC JoltWorldBoundaryShape3D::build([[maybe_unused]] float p_extra_margin) const {
	ERR_FAIL_D_MSG(
		"WorldBoundaryShape3D is not supported by Godot Jolt. "
		"Consider using one or more reasonably sized BoxShape3D instead."
	);
}

Variant JoltSeparationRayShape3D::get_data() const {
	Dictionary data;
	data["length"] = length;
	data["slide_on_slope"] = slide_on_slope;
	return data;
}

void JoltSeparationRayShape3D::set_data(const Variant& p_data) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_length = data.get("length", {});
	ERR_FAIL_COND(maybe_length.get_type() != Variant::FLOAT);

	const Variant maybe_slide_on_slope = data.get("slide_on_slope", {});
	ERR_FAIL_COND(maybe_slide_on_slope.get_type() != Variant::BOOL);

	initialize((float)maybe_length, (bool)maybe_slide_on_slope);
}

bool JoltSeparationRayShape3D::initialize(float p_length, bool p_slide_on_slope) {
	// Godot seems to be forgiving about zero-sized shapes, so we try to mimick that by silently
	// letting these remain invalid.
	if (p_length == 0.0f) {
		return false;
	}

	length = p_length;
	slide_on_slope = p_slide_on_slope;

	return true;
}

void JoltSeparationRayShape3D::clear() {
	jolt_ref = nullptr;
	length = 0.0f;
	slide_on_slope = false;
}

JPH::ShapeRefC JoltSeparationRayShape3D::build(float p_extra_margin) const {
	const JoltRayShapeSettings shape_settings(length + p_extra_margin, slide_on_slope);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to build separation ray shape with length '%f'. "
			"It returned the following error: '%s'.",
			length,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

Variant JoltSphereShape3D::get_data() const {
	return radius;
}

void JoltSphereShape3D::set_data(const Variant& p_data) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	ERR_FAIL_COND(p_data.get_type() != Variant::FLOAT);

	initialize((float)p_data);
}

bool JoltSphereShape3D::initialize(float p_radius) {
	// Godot seems to be forgiving about zero-sized shapes, so we try to mimick that by silently
	// letting these remain invalid.
	if (p_radius <= 0.0f) {
		return false;
	}

	radius = p_radius;

	return true;
}

void JoltSphereShape3D::clear() {
	jolt_ref = nullptr;
	radius = 0.0f;
}

JPH::ShapeRefC JoltSphereShape3D::build(float p_extra_margin) const {
	const JPH::SphereShapeSettings shape_settings(radius + p_extra_margin);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to build sphere shape with radius '%f'. "
			"It returned the following error: '%s'.",
			radius,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

Variant JoltBoxShape3D::get_data() const {
	return half_extents;
}

void JoltBoxShape3D::set_data(const Variant& p_data) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	ERR_FAIL_COND(p_data.get_type() != Variant::VECTOR3);

	initialize((Vector3)p_data);
}

void JoltBoxShape3D::set_margin(float p_margin) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	margin = p_margin;

	initialize(half_extents);
}

bool JoltBoxShape3D::initialize(Vector3 p_half_extents) {
	const float shortest_axis = p_half_extents[p_half_extents.min_axis_index()];

	// Godot seems to be forgiving about zero-sized shapes, so we try to mimick that by silently
	// letting these remain invalid. We also treat anything smaller than or equal to the margin as
	// zero-sized since Jolt will emit errors otherwise.
	if (shortest_axis <= margin) {
		return false;
	}

	half_extents = p_half_extents;

	return true;
}

void JoltBoxShape3D::clear() {
	jolt_ref = nullptr;
	half_extents.zero();
}

JPH::ShapeRefC JoltBoxShape3D::build(float p_extra_margin) const {
	const Vector3 padded_half_extents(
		half_extents.x + p_extra_margin,
		half_extents.y + p_extra_margin,
		half_extents.z + p_extra_margin
	);

	const JPH::BoxShapeSettings shape_settings(to_jolt(padded_half_extents), margin);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to build box shape with half extents '%v'. "
			"It returned the following error: '%s'.",
			half_extents,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

Variant JoltCapsuleShape3D::get_data() const {
	Dictionary data;
	data["height"] = height;
	data["radius"] = radius;
	return data;
}

void JoltCapsuleShape3D::set_data(const Variant& p_data) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_height = data.get("height", {});
	ERR_FAIL_COND(maybe_height.get_type() != Variant::FLOAT);

	const Variant maybe_radius = data.get("radius", {});
	ERR_FAIL_COND(maybe_radius.get_type() != Variant::FLOAT);

	initialize((float)maybe_height, (float)maybe_radius);
}

bool JoltCapsuleShape3D::initialize(float p_height, float p_radius) {
	// Godot seems to be forgiving about zero-sized shapes, so we try to mimick that by silently
	// letting these remain invalid.
	if (p_height <= 0.0f || p_radius <= 0.0f) {
		return false;
	}

	const float half_height = p_height / 2.0f;

	ERR_FAIL_COND_D_MSG(
		half_height < p_radius,
		vformat(
			"Failed to set shape data for capsule shape with height '%f' and radius '%f'. "
			"Half height must be equal to or greater than radius.",
			p_height,
			p_radius
		)
	);

	height = p_height;
	radius = p_radius;

	return true;
}

void JoltCapsuleShape3D::clear() {
	jolt_ref = nullptr;
	height = 0.0f;
	radius = 0.0f;
}

JPH::ShapeRefC JoltCapsuleShape3D::build(float p_extra_margin) const {
	const float half_height = height / 2.0f;
	const float clamped_height = max(half_height - radius, CMP_EPSILON);

	const JPH::CapsuleShapeSettings shape_settings(clamped_height + p_extra_margin, radius);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to build capsule shape with height '%f' and radius '%f'. "
			"It returned the following error: '%s'.",
			height,
			radius,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

Variant JoltCylinderShape3D::get_data() const {
	Dictionary data;
	data["height"] = height;
	data["radius"] = radius;
	return data;
}

void JoltCylinderShape3D::set_data(const Variant& p_data) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_height = data.get("height", {});
	ERR_FAIL_COND(maybe_height.get_type() != Variant::FLOAT);

	const Variant maybe_radius = data.get("radius", {});
	ERR_FAIL_COND(maybe_radius.get_type() != Variant::FLOAT);

	initialize((float)maybe_height, (float)maybe_radius);
}

void JoltCylinderShape3D::set_margin(float p_margin) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	margin = p_margin;

	initialize(height, radius);
}

bool JoltCylinderShape3D::initialize(float p_height, float p_radius) {
	// Godot seems to be forgiving about zero-sized shapes, so we try to mimick that by silently
	// letting these remain invalid. We also treat anything smaller than the margin as zero-sized
	// since Jolt will emit errors otherwise.
	if (p_height < margin || p_radius < margin) {
		return false;
	}

	height = p_height;
	radius = p_radius;

	return true;
}

void JoltCylinderShape3D::clear() {
	jolt_ref = nullptr;
	height = 0.0f;
	radius = 0.0f;
}

JPH::ShapeRefC JoltCylinderShape3D::build(float p_extra_margin) const {
	const float half_height = height / 2.0f;

	const JPH::CylinderShapeSettings shape_settings(
		half_height + p_extra_margin,
		radius + p_extra_margin,
		margin
	);

	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to build cylinder shape with height '%f' and radius '%f'. "
			"It returned the following error: '%s'.",
			height,
			radius,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

Variant JoltConvexPolygonShape3D::get_data() const {
	return vertices;
}

void JoltConvexPolygonShape3D::set_data(const Variant& p_data) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	ERR_FAIL_COND(p_data.get_type() != Variant::PACKED_VECTOR3_ARRAY);

	initialize((PackedVector3Array)p_data);
}

void JoltConvexPolygonShape3D::set_margin(float p_margin) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	margin = p_margin;

	initialize(vertices);
}

bool JoltConvexPolygonShape3D::initialize(PackedVector3Array p_vertices) {
	// Godot seems to be forgiving about zero-sized shapes, so we try to mimick that by silently
	// letting these remain invalid.
	if (p_vertices.size() < 3) {
		return false;
	}

	vertices = std::move(p_vertices);

	return true;
}

void JoltConvexPolygonShape3D::clear() {
	jolt_ref = nullptr;
	vertices.clear();
}

JPH::ShapeRefC JoltConvexPolygonShape3D::build(float p_extra_margin) const {
	const auto vertex_count = (int32_t)vertices.size();

	JPH::Array<JPH::Vec3> jolt_vertices;
	jolt_vertices.reserve((size_t)vertex_count);

	const Vector3* vertices_begin = &vertices[0];
	const Vector3* vertices_end = vertices_begin + vertex_count;

	for (const Vector3* vertex = vertices_begin; vertex != vertices_end; ++vertex) {
		JPH::Vec3& jolt_vertex = jolt_vertices.emplace_back(vertex->x, vertex->y, vertex->z);

		if (p_extra_margin > 0.0f) {
			jolt_vertex += jolt_vertex.NormalizedOr(JPH::Vec3::sZero()) * p_extra_margin;
		}
	}

	const JPH::ConvexHullShapeSettings shape_settings(jolt_vertices, margin);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to build convex polygon shape with vertex count '%d'. "
			"It returned the following error: '%s'.",
			vertex_count,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

Variant JoltConcavePolygonShape3D::get_data() const {
	Dictionary data;
	data["faces"] = faces;
	data["backface_collision"] = backface_collision;
	return data;
}

void JoltConcavePolygonShape3D::set_data(const Variant& p_data) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_faces = data.get("faces", {});
	ERR_FAIL_COND(maybe_faces.get_type() != Variant::PACKED_VECTOR3_ARRAY);

	const Variant maybe_backface_collision = data.get("backface_collision", {});
	ERR_FAIL_COND(maybe_backface_collision.get_type() != Variant::BOOL);

	initialize((PackedVector3Array)maybe_faces, (bool)maybe_backface_collision);
}

bool JoltConcavePolygonShape3D::initialize(PackedVector3Array p_faces, bool p_backface_collision) {
	const auto vertex_count = (size_t)p_faces.size();

	// Godot seems to be forgiving about zero-sized shapes, so we try to mimick that by silently
	// letting these remain invalid.
	if (vertex_count == 0) {
		return false;
	}

	const size_t excess_vertex_count = vertex_count % 3;

	ERR_FAIL_COND_D_MSG(
		excess_vertex_count != 0,
		"Failed to set shape data for concave polygon shape with vertex count '{}'. "
		"Expected a vertex count divisible by 3."
	);

	const size_t face_count = vertex_count / 3;

	if (face_count == 0) {
		return false;
	}

	faces = std::move(p_faces);
	backface_collision = p_backface_collision;

	return true;
}

void JoltConcavePolygonShape3D::clear() {
	jolt_ref = nullptr;
	faces.clear();
	backface_collision = false;
}

JPH::ShapeRefC JoltConcavePolygonShape3D::build([[maybe_unused]] float p_extra_margin) const {
	if (unlikely(p_extra_margin > 0.0f)) {
		WARN_PRINT(
			"Concave polygon shapes with extra margin are not supported by Godot Jolt."
			"Any such value will be ignored."
		);
	}

	const auto vertex_count = (int32_t)faces.size();
	const int32_t face_count = vertex_count / 3;

	JPH::TriangleList jolt_faces;
	jolt_faces.reserve((size_t)face_count);

	const Vector3* faces_begin = &faces[0];
	const Vector3* faces_end = faces_begin + vertex_count;

	for (const Vector3* vertex = faces_begin; vertex != faces_end; vertex += 3) {
		const Vector3* v0 = vertex + 0;
		const Vector3* v1 = vertex + 1;
		const Vector3* v2 = vertex + 2;

		jolt_faces.emplace_back(
			JPH::Float3(v2->x, v2->y, v2->z),
			JPH::Float3(v1->x, v1->y, v1->z),
			JPH::Float3(v0->x, v0->y, v0->z)
		);

		if (backface_collision) {
			jolt_faces.emplace_back(
				JPH::Float3(v0->x, v0->y, v0->z),
				JPH::Float3(v1->x, v1->y, v1->z),
				JPH::Float3(v2->x, v2->y, v2->z)
			);
		}
	}

	const JPH::MeshShapeSettings shape_settings(jolt_faces);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to build concave polygon shape with vertex count '%d'. "
			"It returned the following error: '%s'.",
			vertex_count,
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

Variant JoltHeightMapShape3D::get_data() const {
	Dictionary data;
	data["width"] = width;
	data["depth"] = depth;
	data["heights"] = heights;
	return data;
}

void JoltHeightMapShape3D::set_data(const Variant& p_data) {
	ON_SCOPE_EXIT {
		shape_changed();
	};

	clear();

	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_heights = data.get("heights", {});
	ERR_FAIL_COND(maybe_heights.get_type() != Variant::PACKED_FLOAT32_ARRAY);

	const Variant maybe_width = data.get("width", {});
	ERR_FAIL_COND(maybe_width.get_type() != Variant::INT);

	const Variant maybe_depth = data.get("depth", {});
	ERR_FAIL_COND(maybe_depth.get_type() != Variant::INT);

	initialize((PackedFloat32Array)maybe_heights, (int32_t)maybe_width, (int32_t)maybe_depth);
}

bool JoltHeightMapShape3D::initialize(
	PackedFloat32Array p_heights,
	int32_t p_width,
	int32_t p_depth
) {
	const auto height_count = (int32_t)p_heights.size();

	if (height_count == 0) {
		return false;
	}

	// HACK(mihe): A height map shape will have a width or depth of 2 while it's transitioning from
	// its default state. Since Jolt doesn't support non-square height maps, and it's unlikely that
	// anyone would actually want a height map of such small dimensions, we silently let this remain
	// invalid in order to not display an error every single time we create a shape of this type.
	if (p_width <= 2 || p_depth <= 2) {
		return false;
	}

	ERR_FAIL_COND_D_MSG(
		height_count != p_width * p_depth,
		vformat(
			"Failed to set shape data for height map shape with width '%d', depth '%d' and height "
			"count '%d'. Height count must be equal to width multiplied by depth.",
			p_width,
			p_depth,
			height_count
		)
	);

	ERR_FAIL_COND_D_MSG(
		p_width != p_depth,
		vformat(
			"Failed to set shape data for height map shape with width '%d', depth '%d' and height "
			"count '%d'. Height maps with differing width and depth are not supported by Godot "
			"Jolt.",
			p_width,
			p_depth,
			height_count
		)
	);

	const auto sample_count = (JPH::uint32)p_width;

	ERR_FAIL_COND_D_MSG(
		!is_power_of_2(sample_count),
		vformat(
			"Failed to set shape data for height map shape with width '%d', depth '%d' and height "
			"count '%d'. Height maps with a width/depth that is not a power of two are not "
			"supported by Godot Jolt.",
			p_width,
			p_depth,
			height_count
		)
	);

	heights = std::move(p_heights);
	width = p_width;
	depth = p_depth;

	return true;
}

void JoltHeightMapShape3D::clear() {
	jolt_ref = nullptr;
	heights.clear();
	width = 0;
	depth = 0;
}

JPH::ShapeRefC JoltHeightMapShape3D::build([[maybe_unused]] float p_extra_margin) const {
	if (unlikely(p_extra_margin > 0.0f)) {
		WARN_PRINT(
			"Height map shapes with extra margin are not supported by Godot Jolt. "
			"Any such value will be ignored."
		);
	}

	const int32_t width_tiles = width - 1;
	const int32_t depth_tiles = depth - 1;

	const float half_width_tiles = (float)width_tiles / 2.0f;
	const float half_depth_tiles = (float)depth_tiles / 2.0f;

	const JPH::HeightFieldShapeSettings shape_settings(
		heights.ptr(),
		JPH::Vec3(-half_width_tiles, 0, -half_depth_tiles),
		JPH::Vec3::sReplicate(1.0f),
		(JPH::uint32)width
	);

	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();

	ERR_FAIL_COND_D_MSG(
		shape_result.HasError(),
		vformat(
			"Failed to build height map shape with width '%d', depth '%d' and height count '%d'. "
			"It returned the following error: '%s'.",
			width,
			depth,
			heights.size(),
			to_godot(shape_result.GetError())
		)
	);

	return shape_result.Get();
}

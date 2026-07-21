#pragma once
#include "aabb.hpp"
#include "hit_record.hpp"
#include "hittable.hpp"
#include "ray.hpp"
#include "vec3.hpp"
#include <memory>

class translate : public hittable {
    public:
        translate(std::shared_ptr<hittable> object, const vec3& offset) : object(std::move(object)), offset(offset) {
            bbox = this->object->bounding_box() + offset;
        }

        [[nodiscard]] aabb bounding_box() const override { return bbox; }

        [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
            const ray offset_r(r.origin() - offset, r.direction(), r.time());

            if (!object->hit(offset_r, ray_t, rec)) return false;
            
            rec.p += offset;
            return true;
        }
    private:
        std::shared_ptr<hittable> object;
        vec3 offset;
        aabb bbox;
};
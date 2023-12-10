#version 410
#define Epsilon 0.000001

// uniform vec3 light_position;
uniform vec3 camera_position;
uniform sampler3D volume;
uniform float voxel_size;
uniform ivec3 grid_size;
uniform vec3 light_direction;

// world space
flat in float face_dot_v;

// model space
flat in vec3 model_cam_pos;
in vec3 fV;
in vec3 pos;

//out
out vec4 fColor;


float isInside(vec3 pos){
    // this works but did not notice performance difference
/*
    return step(0.0, pos.x)*step(pos.x, 1.0) *
    step(0.0, pos.y)*step(pos.y, 1.0) *
    step(0.0, pos.z)*step(pos.z, 1.0);
*/
        if (pos.x < 0.0 || pos.x > 1.0) return 0.0;
        if (pos.y < 0.0 || pos.y > 1.0) return 0.0;
        if (pos.z < 0.0 || pos.z > 1.0) return 0.0;
        return 1.0;
}

struct start_t{
    vec3 pos;
    vec3 normal;
};

vec3 uvw_to_normal(vec3 uvw){
    // find normal
    vec3 hit_to_center = uvw - vec3(0.5);
    vec3 abs_hit_to_center = abs(hit_to_center);
    float max_component = max(max(abs_hit_to_center.x, abs_hit_to_center.y), abs_hit_to_center.z);

    vec3 normal;
    if (abs_hit_to_center.x == max_component) {
        normal = vec3(sign(hit_to_center.x), 0.0, 0.0);
    } else if (abs_hit_to_center.y == max_component) {
        normal = vec3(0.0, sign(hit_to_center.y), 0.0);
    } else {
        normal = vec3(0.0, 0.0, sign(hit_to_center.z));
    }
    return normal;
}

vec2 uvw_to_uv(vec3 uvw){
    if(uvw.x == 0.0 && uvw.y == 0.0 && uvw.z == 0.0) return vec2(0.0, 0.0);
    if(uvw.x == 1.0 && uvw.y == 1.0 && uvw.z == 1.0) return vec2(1.0, 1.0);
    if(uvw.x == 0.0 && uvw.y == 1.0 && uvw.z == 1.0) return vec2(0.0, 1.0);
    if(uvw.x == 1.0 && uvw.y == 0.0 && uvw.z == 1.0) return vec2(1.0, 0.0);
    if(uvw.x == 1.0 && uvw.y == 1.0 && uvw.z == 0.0) return vec2(1.0, 0.0);
    if(uvw.x == 0.0 && uvw.y == 0.0 && uvw.z == 1.0) return vec2(0.0, 1.0);
    vec3 tangents = vec3(1) - abs(uvw_to_normal(uvw));
    vec3 a, b;
    if (tangents.x == 0.0f) {
        a = vec3(0, 1, 0);
        b = vec3(0, 0, 1);
    } else if (tangents.y == 0.0f) {
        a = vec3(1, 0, 0);
        b = vec3(0, 0, 1);
    } else {
        a = vec3(1, 0, 0);
        b = vec3(0, 1, 0);
    }
    return vec2(dot(b, uvw), dot(a, uvw));
}

// Work In Progress
start_t findStartPos(){
    vec3 dir = normalize(fV);
    // x/0 is undefined behaviour
    //if(dir.x == 0.0) dir.x = 0.0000001;
    //if(dir.y == 0.0) dir.y = 0.0000001;
    //if(dir.z == 0.0) dir.z = 0.0000001;
    vec3 dir_inv = vec3(1.0)/dir;
    // move origin up to before intersecting the box
    vec3 origin = pos - 2.0 * dir;
    float t1 = (0.0 - origin.x) * dir_inv.x;
    float t2 = (1.0 - origin.x) * dir_inv.x;
    float t3 = (0.0 - origin.y) * dir_inv.y;
    float t4 = (1.0 - origin.y) * dir_inv.y;
    float t5 = (0.0 - origin.z) * dir_inv.z;
    float t6 = (1.0 - origin.z) * dir_inv.z;

    float tmin = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));
    float tmax = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));

    // we are sure that we hit the box otherwise we would not render here!
    // if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behing us
    //if (tmax < 0) return { true};
    // if tmin > tmax, ray doesn't intersect AABB
    //if (tmin > tmax) return { false};


    // first intersection with cube
    vec3 near =  origin + dir * (tmin + Epsilon * 10.0f);
    vec3 far =  origin + dir * (tmax - Epsilon);
    // if camera is closer to the pos on the backface than the intersect, return the camera pos
    if (length(near-pos) > length(model_cam_pos - pos)){
        return start_t(model_cam_pos, vec3(0, 0, 0));// 0.0 - 1.0
    }
    return start_t(near, uvw_to_normal(near));
}
struct hit_t {
    float depth;
    vec3 voxel_pos;
    vec3 pixel_pos;
    vec3 uvw;
    vec2 uv;
    vec3 normal;
    int material;
};

hit_t fixed_step(){
    vec3 V = normalize(fV) * voxel_size/15;// fixed step
    start_t start = findStartPos();// P is in 0-1.0 space
    vec3 P = start.pos;
    int max_step = int(15*15/voxel_size);
    for (int i = 0; i < max_step; i++){
        if (isInside(P) < 0.5) discard;
        int material = int(round(texture(volume, P).r*255));
        if (material != 0) {
            float t = length(P - start.pos);
            vec3 index = floor(P/voxel_size);
            return hit_t(t, index, P, (P - vec3(index))/voxel_size, uvw_to_uv((P - vec3(index))/voxel_size), start.normal, material);
        }
        P += V;
    }
    discard;
}

float luminance(vec3 v)
{
    return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}
vec3 reinhard_jodie(vec3 v)
{
    float l = luminance(v);
    vec3 tv = v / (1.0f + v);
    return mix(v / (1.0f + l), tv, tv);
}
// have not tried yet
vec3 shade(hit_t hit){
    vec3 V=normalize(fV);
    vec3 N=normalize(hit.normal);
    vec3 L=normalize(light_direction);
    float diffuse_co= 1.7f * max(dot(L, N), 0);
    float specular_co = 0.0;
    bool blinn = true;
    if (blinn)
    {
        vec3 halfwayDir = normalize(L -V);
        specular_co = pow(max(dot(N, halfwayDir), 0.0), 4.0);
    }
    else
    {
        vec3 R = reflect(-L, N);
        specular_co = pow(max(dot(-V, R), 0.0), 1.1);
    }
    //vec3 voxel_color=vec3(1, 0, 0);
    vec3 mat_color = vec3(hit.material/255.0, 0, 0);
    vec3 ambient=vec3(0.4) * mat_color;
    vec3 v = ambient + diffuse_co * mat_color + specular_co * vec3(0.6);//+specular_co*vec3(0, 0, 1);
    return reinhard_jodie(v);
}

// finally works omg
// got some help from here
// https://www.shadertoy.com/view/4dS3RG
hit_t fvta_step(){
    start_t start = findStartPos();
    vec3 ro = start.pos;
    vec3 normal = start.normal;
    vec3 rd = normalize(fV);
    vec3 voxel_index = floor(ro * (1./voxel_size));
    vec3 voxel_pos = voxel_size * voxel_index;
    vec3 rs = sign(rd);
    vec3 deltaDist = voxel_size/rd;
    vec3 sideDist = ((voxel_pos-ro)/voxel_size + 0.5 + rs * 0.5) * deltaDist;
    //voxel_pos = voxel_size * (voxel_index + 0.1);
    int max_steps = int(3.0/voxel_size);

    vec3 final_pos = ro * 1./voxel_size;
    float t = 0.0;
    vec3 pos = ro;//voxel_size * (voxel_index + 0.1);
    vec3 uvw = (pos - voxel_pos)/voxel_size;
    vec2 uv = uvw_to_uv(uvw);

    for (int i = 0; i < max_steps; i++){
        if (isInside(pos) < 0.5) {
            //return hit_t(t, voxel_pos, pos, uvw, uv, vec3(1,0,0), 0);
            discard;
        }
        int mat = int(round(texture(volume, pos).r*255));
        if (mat > 0){
            return hit_t(t, voxel_pos, pos, uvw, uv, normal, mat);
        }
        /*
                if (sideDist.x < sideDist.y && sideDist.x < sideDist.z) {
                    // X-axis traversal.
                    normal = -vec3(1, 0, 0) * rs;
                    t = sideDist.x * rd;
                } else if (sideDist.y < sideDist.z) {
                    // Y-axis traversal.
                    normal = -vec3(0, 1, 0) * rs;
                    t = sideDist.y * rd;
                } else {
                    // Z-axis traversal.
                    normal = -vec3(0, 0, 1) * rs;
                    t = sideDist.z * rd;
                }
        */
        // black magic compare between sideDist.x < sideDist.y && sideDist.x < sideDist.z etc
        vec3 mm = step(sideDist.xyz, sideDist.yxy) * step(sideDist.xyz, sideDist.zzx);
        normal = -mm * rs;

        voxel_pos += voxel_size * -normal;

        // other stuff that is nice to know
        vec3 mini = ((voxel_pos-ro)/voxel_size + 0.5 - 0.5*vec3(rs))*deltaDist;
        t = max (mini.x, max (mini.y, mini.z));
        pos = ro + rd * (t + Epsilon);
        uvw = (pos - voxel_pos)/voxel_size;
        uv = vec2(dot(mm.yzx, uvw), dot(mm.zxy, uvw));

        sideDist += -normal * deltaDist;
    }
    discard;
}
mat4 rotationX(in float angle) {
    return mat4(1.0, 0, 0, 0,
    0, cos(angle), -sin(angle), 0,
    0, sin(angle), cos(angle), 0,
    0, 0, 0, 1);
}

mat4 rotationY(in float angle) {
    return mat4(cos(angle), 0, sin(angle), 0,
    0, 1.0, 0, 0,
    -sin(angle), 0, cos(angle), 0,
    0, 0, 0, 1);
}

mat4 rotationZ(in float angle) {
    return mat4(cos(angle), -sin(angle), 0, 0,
    sin(angle), cos(angle), 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1);
}
float ao(hit_t hit){
    vec3 N=normalize(hit.normal);
    vec3 P = hit.pixel_pos;
    float ao_inv = 1.0;
    int nbr_samples = 10;
    float r = voxel_size * 0.2;
    vec3 sphere_center = P + N * r;
    vec3 bitangent = cross(N, vec3(0.5, 0.5, 0.5));
    for (int i = 0; i < nbr_samples; i++){
        // rotate bitangent
        float angle = 2.0 * 3.14159265359 * float(i)/float(nbr_samples);
        bitangent = normalize((rotationZ(angle * N.z) * rotationX(angle * N.x) * rotationY(angle * N.y) * vec4(bitangent, 1.0)).xyz);
        vec3 sample_point =  sphere_center + bitangent * r*0.5;
        if (isInside(sample_point) < 0.5) continue;
        float material = texture(volume, sample_point).r * 255.0;
        if (material > 0.0){
            ao_inv -= 1.0/(nbr_samples+1.0);
        }
    }
    return ao_inv;
}

void main()
{
    // custom front face culling to do it based on cam pos
    //if (face_dot_v < 0.0) discard;

    //vec3 color = findStartPos();
    hit_t hit;
    //hit = fixed_step();
    hit = fvta_step();
    if (isInside(hit.pixel_pos - 0.01 * hit.normal * voxel_size) < 0.5){
        discard;
    }
    vec3 color = vec3(hit.material/255.0, 0, 0);
    color = shade(hit);
    //color *= ao(hit);
    //color = hit.pixel_pos;
    //color = hit.voxel_pos;
    //color = normalize(hit.normal) * 0.5 + 0.5;
    //color = hit.uvw;
    //color = vec3(hit.uv, 1.0);
    //color = vec3(hit.depth);


    fColor = vec4(color, 1.0);
}


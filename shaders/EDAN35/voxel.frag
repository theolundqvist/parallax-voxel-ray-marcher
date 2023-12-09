#version 410
#define Epsilon 0.0000001

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
    return step(0.0, pos.x)*step(pos.x, 1.0) *
    step(0.0, pos.y)*step(pos.y, 1.0) *
    step(0.0, pos.z)*step(pos.z, 1.0);
    /*
        if (pos.x < 0.0 || pos.x > 1.0) return false;
        if (pos.y < 0.0 || pos.y > 1.0) return false;
        if (pos.z < 0.0 || pos.z > 1.0) return false;
        return true;
    */
}

// Work In Progress
vec3 findStartPos(){
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
    vec3 intersect =  origin + dir * (tmin + 0.00001);
    // if camera is closer to the pos on the backface than the intersect, return the camera pos
    if (length(intersect-pos) > length(model_cam_pos - pos)){
        return model_cam_pos;// 0.0 - 1.0
    }
    return intersect;
}

struct hit_t {
    vec3 voxel;
    vec3 normal;
    int material;
};

hit_t fixed_step(){
    vec3 V = normalize(fV) * voxel_size/15;// fixed step
    vec3 P = findStartPos();// P is in 0-1.0 space
    vec3 last_voxel = floor(P * voxel_size);
    int max_step = int(15*15/voxel_size);
    for (int i = 0; i < max_step; i++){
        if (isInside(P) < 0.5) discard;
        vec3 norm = floor(P * voxel_size) - last_voxel;
        last_voxel -= norm;
        int material = int(round(texture(volume, P).r*255));
        if (material != 0) return hit_t(P, norm, material);
        P += V;
    }
    discard;
}


// have not tried yet
vec3 shade(hit_t hit){
    vec3 V=normalize(fV);
    vec3 N=normalize(hit.voxel-hit.normal);
    vec3 L=normalize(light_direction);

    float diffuse_co=max(dot(L, N), 0);
    vec3 R=normalize(reflect(-L, N));
    float specular_co=pow(max(dot(R, V), 0.0), 0.5);
    //vec3 voxel_color=vec3(1, 0, 0);
    vec3 voxel_color=diffuse_co*vec3(0, 1, 0)+specular_co*vec3(0, 0, 1);//+specular_co*vec3(0, 0, 1);
    // use material color
    return voxel_color;
}

// finally works omg
// got some help from here
// https://www.shadertoy.com/view/4dS3RG
hit_t fvta_step(){
    vec3 ro = findStartPos();
    vec3 rd = normalize(fV);
    vec3 voxel_pos = voxel_size * floor(ro * (1./voxel_size));
    vec3 rs = sign(rd);
    vec3 deltaDist = voxel_size/rd;
    vec3 sideDist = ((voxel_pos-ro)/voxel_size + 0.5 + rs * 0.5) * deltaDist;
    voxel_pos = voxel_size * (floor(ro * (1./voxel_size)) + 0.1);
    int max_steps = int(3.0/voxel_size);
    for (int i = 0; i < max_steps; i++){
        if (isInside(voxel_pos) < 0.5) {
            discard;
        }
        int mat = int(round(texture(volume, voxel_pos).r*255));
        // black magic compare between sideDist.x < sideDist.y && sideDist.x < sideDist.z etc
        vec3 mm = step(sideDist.xyz, sideDist.yxy)  * step(sideDist.xyz, sideDist.zzx);
        vec3 norm = -mm * rs;
        if (mat > 0){
            return hit_t(voxel_pos, norm, mat);
        }
        voxel_pos += voxel_size * -norm;
        sideDist += -norm * deltaDist;
    }
    discard;
}



void main()
{
    // custom front face culling to do it based on cam pos
    if (face_dot_v < 0.0) discard;

    //vec3 color = findStartPos();
    hit_t hit;
    //hit = fixed_step();
    hit = fvta_step();

    vec3 color = vec3(hit.voxel);
    //color = shade(hit);
    //color = normalize(hit.last_voxel - hit.voxel);

    fColor = vec4(color, 1.0);
}


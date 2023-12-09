#version 410

// uniform vec3 light_position;
uniform vec3 camera_position;
uniform sampler3D volume;
uniform float voxel_size;
uniform ivec3 grid_size;


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
    if(length(intersect-pos) > length(model_cam_pos - pos))
        return model_cam_pos; // 0.0 - 1.0
    return intersect;
}

vec3 fixed_step(){
    vec3 V = normalize(fV) * voxel_size/15;// fixed step
    vec3 P = findStartPos(); // P is in 0-1.0 space
    for (int i = 0; i < 700; i++){
        if (isInside(P) < 0.5) discard;
        int material = int(round(texture(volume, P).r*255));
        if (material != 0) return vec3(float(material)/255, 0, 0);
        P += V;
    }
    discard;
}

vec3 fvta_step(){
    //replaced by FVTA
    //initialization
    vec3 start = findStartPos();
    vec3 current_voxel=vec3(floor(start.x/voxel_size), floor(start.y/voxel_size), floor(start.z/voxel_size));//start from current point, this is the id of the current voxel
    vec3 V=normalize(fV);

    float cos_x=V.x;
    float cos_y=V.y;
    float cos_z=V.z;


    float stepX = (V.x >= 0) ? 1:-1;// step longth of X. 1 for incremented and -1 for decremented.
    float stepY = (V.y >= 0) ? 1:-1;// step longth of Y
    float stepZ = (V.z >= 0) ? 1:-1;// step longth of Z

    float next_voxel_x = (current_voxel.x+stepX)*voxel_size;// find the startition of next voxel's boundary.
    float next_voxel_y = (current_voxel.y+stepY)*voxel_size;//
    float next_voxel_z = (current_voxel.z+stepZ)*voxel_size;//

    float tMaxX = (cos_x!=0) ? (next_voxel_x - start.x)/cos_x:200000000;//find the t at which the ray crosses the first vertical voxel boundary
    float tMaxY = (cos_y!=0) ? (next_voxel_y - start.y)/cos_y:200000000;//and the mininum distance of x,y,z is the first voxel that the ray hit
    float tMaxZ = (cos_z!=0) ? (next_voxel_z - start.z)/cos_z:200000000;//

    float tDeltaX = (cos_x!=0) ? voxel_size/cos_x*stepX : 200000000;//length of step
    float tDeltaY = (cos_y!=0) ? voxel_size/cos_y*stepY : 200000000;
    float tDeltaZ = (cos_z!=0) ? voxel_size/cos_z*stepZ : 200000000;

    for (int i=0;i<300;i++)//need to add the end point
    {
        if (tMaxX==tMaxY &&  tMaxY ==tMaxZ)
        {
            current_voxel=vec3(0.0, 0.0, 0.0);
            int material = int(round(texture(volume, current_voxel).r*255));
            if (material != 0) return vec3(float(material)/255, 0, 0);
        }
        else
        {

            if (tMaxX < tMaxY)
            {
                if (tMaxX < tMaxZ)
                {
                    current_voxel[0] += stepX;
                    tMaxX += tDeltaX;
                }
                else
                {
                    current_voxel[2] += stepZ;
                    tMaxZ += tDeltaZ;
                }
            }
            else
            {
                if (tMaxY < tMaxZ)
                {
                    current_voxel[1] += stepY;
                    tMaxY += tDeltaY;
                }
                else
                {
                    current_voxel[2] += stepZ;
                    tMaxZ += tDeltaZ;
                }
            }
            vec3 current_voxel_position=vec3(current_voxel.x*voxel_size, current_voxel.y*voxel_size, current_voxel.z*voxel_size);//only render the front face
            if (current_voxel_position.x<0 || current_voxel_position.y<0||current_voxel_position.z<0 || current_voxel_position.x>1 || current_voxel_position.y>1 || current_voxel_position.z>1)
            {
                discard;
            }
            else
            {
                int material = int(round(texture(volume, current_voxel_position).r*255));
                if (material != 0) return vec3(float(material)/255, 0, 0);
            }
        }
    }
    discard;
}



void main()
{
    // custom front face culling to do it based on cam pos
    if(face_dot_v < 0.0) discard;

    //vec3 color = findStartPos();
    //vec3 color = fixed_step();
    vec3 color = fvta_step();

    fColor = vec4(color, 1);
}


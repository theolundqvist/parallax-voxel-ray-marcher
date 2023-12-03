#version 410

// uniform vec3 light_position;
uniform vec3 camera_position;
uniform sampler3D volume;
uniform float voxel_size;
uniform ivec3 grid_size;


in vec3 fV;
in vec3 pos;
out vec4 fColor;


bool isInside(vec3 pos){
    if (pos.x < 0.0 || pos.x > 1.0) return false;
    if (pos.y < 0.0 || pos.y > 1.0) return false;
    if (pos.z < 0.0 || pos.z > 1.0) return false;
    return true;
}

// Work In Progress
vec3 findStartPos(){
    vec3 rayOrigin = pos;// Assuming world coords for simplicity
    vec3 rayDir = -fV;//normalize(pos - camera_position);// Direction from cam to current pixel
    rayDir.x = -rayDir.x;// ???

    // Example volume bounds, assuming the volume is axis-aligned and starts at (0,0,0) with size equal to "grid_size"
    vec3 minBounds = vec3(0.0);
    vec3 maxBounds = vec3(1.0f);

    // Initialize variables to hold the near and far intersection distances along the ray
    float tNear = -999999.0;
    float tFar = 999999.0;

    // Loop over each axis to find intersections with the volume bounds
    for (int i = 0; i < 3; ++i) {
        // Determine intersections with the slabs
        float t1 = (minBounds[i] - rayOrigin[i]) / rayDir[i];
        float t2 = (maxBounds[i] - rayOrigin[i]) / rayDir[i];

        // Ensure t1 is the nearer intersection
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }

        // Update the 'near' and 'far' distances for the ray
        tNear = max(tNear, t1);
        tFar = min(tFar, t2);

        // If the distances 'cross', there is no intersection
        if (tNear > tFar || tFar < 0.0) {
            //return vec3(-1.0);// No intersection, return some invalid position
        }
    }

    // If we have a valid intersection, calculate the exact starting position
    vec3 startPos = rayOrigin + tNear * rayDir;
    return startPos;
}

vec3 fixed_step(){
    vec3 V = normalize(fV) * voxel_size/15;// fixed step
    V.x = -V.x;// ???
    vec3 P = pos;
    for (int i = 0; i < 400; i++){
        if (!isInside(P)) discard;
        int material = int(round(texture(volume, P).r*255));
        if (material != 0) return vec3(float(material)/255, 0, 0);
        P += V;
    }
    discard;
}

vec3 fvta_step(){
//replaced by FVTA
//initialization
vec3 current_voxel=vec3(floor(pos.x/voxel_size),floor(pos.y/voxel_size),floor(pos.z/voxel_size));//start from current point, this is the id of the current voxel
vec3 V=normalize(fV);
V.x = -V.x;

float cos_x=V.x;
float cos_y=V.y;
float cos_z=V.z;


float stepX = (V.x >= 0) ? 1:-1; // step longth of X. 1 for incremented and -1 for decremented.
float stepY = (V.y >= 0) ? 1:-1; // step longth of Y
float stepZ = (V.z >= 0) ? 1:-1; // step longth of Z

float next_voxel_x = (current_voxel.x+stepX)*voxel_size; // find the position of next voxel's boundary.
float next_voxel_y = (current_voxel.y+stepY)*voxel_size; //
float next_voxel_z = (current_voxel.z+stepZ)*voxel_size; //

float tMaxX = (cos_x!=0) ? (next_voxel_x - pos.x)/cos_x:200000000 ; //find the t at which the ray crosses the first vertical voxel boundary
float tMaxY = (cos_y!=0) ? (next_voxel_y - pos.y)/cos_y:200000000 ; //and the mininum distance of x,y,z is the first voxel that the ray hit
float tMaxZ = (cos_z!=0) ? (next_voxel_z - pos.z)/cos_z:200000000 ; //

float tDeltaX = (cos_x!=0) ? voxel_size/cos_x*stepX : 200000000 ;//length of step
float tDeltaY = (cos_y!=0) ? voxel_size/cos_y*stepY : 200000000 ;
float tDeltaZ = (cos_z!=0) ? voxel_size/cos_z*stepZ : 200000000 ;

for(int i=0;i<300;i++)//need to add the end point
{
    if(tMaxX==tMaxY &&  tMaxY ==tMaxZ)
    {
        current_voxel=vec3(0.0,0.0,0.0);
        int material = int(round(texture(volume, current_voxel).r*255));
        if(material != 0) return vec3(float(material)/255, 0, 0);
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
        vec3 current_voxel_position=vec3(current_voxel.x*voxel_size,current_voxel.y*voxel_size,current_voxel.z*voxel_size);//only render the front face
        if(current_voxel_position.x<0 || current_voxel_position.y<0||current_voxel_position.z<0 || current_voxel_position.x>1 || current_voxel_position.y>1 || current_voxel_position.z>1)
        {
            discard;
        }
        else
        {
            int material = int(round(texture(volume, current_voxel_position).r*255));
            if(material != 0) return vec3(float(material)/255, 0, 0);
        }
    }
}
discard;
}



void main()
{


    //vec3 color = pos;
    //vec3 color = texture(volume, pos).rgb;
    //vec3 color = vec3(1.0, 1.0, 1.0);
    //vec3 color = fV;

    vec3 color = fixed_step();
    //vec3 color = fvta_step();

    fColor = vec4(color, 1);
}


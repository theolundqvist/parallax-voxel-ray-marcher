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

vec3 trace(){
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



void main()
{


    //vec3 color = pos;
    //vec3 color = texture(volume, pos).rgb;
    //vec3 color = vec3(1.0, 1.0, 1.0);
    //vec3 color = fV;
    vec3 color = trace();

    fColor = vec4(color, 1);
}


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
  if(pos.x < 0.0 || pos.x > 1.0) return false;
  if(pos.y < 0.0 || pos.y > 1.0) return false;
  if(pos.z < 0.0 || pos.z > 1.0) return false;
  return true;
}

vec3 trace(){
  vec3 V = normalize(fV) * voxel_size/15; // fixed step
  V.x = -V.x; // ???
  vec3 P = pos;
  for(int i = 0; i < 400; i++){
    if(!isInside(P)) discard;
		int material = int(round(texture(volume, P).r*255));
    if(material != 0) return vec3(float(material)/255, 0, 0);
		P += V;
	}
  discard;
}



void main()
{


  //vec3 color = pos;
  vec3 color = texture(volume, pos).rgb;
  //vec3 color = vec3(1.0, 1.0, 1.0);

  fColor = vec4(color, 1);
}


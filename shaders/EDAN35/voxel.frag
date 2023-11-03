#version 410

// uniform vec3 light_position;
uniform vec3 camera_position;
uniform sampler3D voxels;
uniform float voxel_size;
uniform ivec3 grid_size;




in vec3 fV;
in vec3 pos;

out vec4 fColor;

int insideBox3D(vec3 v, vec3 bottomLeft, vec3 topRight) {
    vec3 s = step(bottomLeft, v) - step(topRight, v);
    return int(s.x * s.y * s.z); 
}

int trace(){
  vec3 V = -normalize(fV) * voxel_size/6; // fixed step
  vec3 P = pos;
  for(int i = 0; i < 500; i++){
    if(insideBox3D(pos, vec3(0,0,0), vec3(grid_size)) == 0) return 0;
		int material = int(round(texture(voxels, P).r*255));
		if(material != 0) return material;
		P += V;
	}
	return 0;
}



void main()
{


  int material = trace();



	if(material == 0) discard;

  fColor = vec4(vec3(float(material)/255, 0, 0), 1);

	// fColor = vec4(V*0.5+0.5, 1);
}


#version 430 core
#define PI 3.14159265359

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform float R;
uniform int n;
uniform float bias;
uniform float s;
uniform float k;


void main () {

	ivec2 size = textureSize(gPosition, 0);
	int width = size.x;
	int height = size.y;
	
	float S = 0.0f;

	vec3 P = texture(gPosition, TexCoords).xyz;
	float d = texture(gPosition, TexCoords).w;
	
	vec3 N = texture(gNormal, TexCoords).xyz;

	int x_prime = int(gl_FragCoord.x); 
	int y_prime = int(gl_FragCoord.y);

	float x = float(x_prime) / width;
	float y = float(y_prime) / height;
			
	//int phi = (( 30 * x_prime) ^ y_prime) + (10 * x_prime * y_prime);
	float phi = PI * fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
	float c = 0.1 * R;

	for (int i = 0; i < n; i++) {
	
		float alpha = float(i + 0.5) / float(n);
		float h = alpha * R / d;
		float theta = 2 * PI * alpha * (7.0 * float(n) / 9.0) + phi;

		vec2 samplePos = vec2(x, y) + (vec2(cos(theta), sin(theta)) * h);
		   
		vec3 Pi = texture(gPosition, samplePos).xyz;
		float di = texture(gPosition, samplePos).w;


		vec3 omega = Pi - P;
		float H = step(0.0, R - length(omega));


		float num = max(0.0f, dot(N, omega) - (bias)) * H;
		float denom = max( c * c, dot(omega, omega));

		S += ( (2 * PI * c) / float(n) ) * (num / denom) ;

	}

	float A = pow(max(0.0, 1.0 - s * S), k);
	FragColor = vec4(A, 0.0, 0.0, 1.0);
}

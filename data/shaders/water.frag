#version 330 core

in vec3 fWPos;
in vec3 fNormal;
in vec2 fUV;
in float fFog;

uniform float time;
uniform vec3 eyePos;
uniform sampler2D textureDuDv;
uniform samplerCube textureReflection;
uniform sampler2D textureRefraction;

float fresnel(float n1, float n2, vec3 normal, vec3 incident) {
	// Schlick aproximation
	/*float r0 = (n1-n2) / (n1+n2);
	r0 *= r0;
	float cosX = -dot(normal, incident);
	if (n1 > n2)
	{
		float n = n1/n2;
		float sinT2 = n*n*(1.0-cosX*cosX);
		// Total internal reflection
		if (sinT2 > 1.0f)
			return 1.0f;
		cosX = sqrt(1.0f-sinT2);
	}
	float x = 1.0f - cosX;
	float fres = r0 + (1.0f-r0) * pow(x, 5);
	return fres;*/
	
	float f = 1.0 - pow(dot(normal, -incident), 2.0);
	return min(1.0, pow(f, 10) * 1.3);
 }

void main() {
	vec3 normal = vec3(0.0, 1.0, 0.0);

	float changeSpeed = 0.02;
	float change = time * changeSpeed;

	float perturbFreq1 = 0.5;
	float perturbStrength1 = 0.1;
	vec4 dudv = texture(textureDuDv, (fUV + change) * perturbFreq1) * 2.0 - 1.0;
	vec3 perturb1 = dudv.rbg * perturbStrength1;

	float perturbFreq2 = 2.0;
	float perturbStrength2 = 0.05;
	dudv = texture(textureDuDv, (fUV - change) * perturbFreq2) * 2.0 - 1.0;
	vec3 perturb2 = dudv.rbg * perturbStrength2;

	float perturbFreq3 = 8.0;
	float perturbStrength3 = 0.03;
	dudv = texture(textureDuDv, (fUV.yx + change) * perturbFreq3) * 2.0 - 1.0;
	vec3 perturb3 = dudv.rbg * perturbStrength3;

	float perturbFreq4 = 20.0;
	float perturbStrength4 = 0.02;
	dudv = texture(textureDuDv, (fUV.yx + change/5) * perturbFreq4) * 2.0 - 1.0;
	vec3 perturb4 = dudv.rbg * perturbStrength4;

	float perturbFreq5 = 60.0;
	float perturbStrength5 = 0.05;
	dudv = texture(textureDuDv, (fUV.yx - change/7) * perturbFreq5) * 2.0 - 1.0;
	vec3 perturb5 = dudv.rbg * perturbStrength5;

	float perturbTotalFactor = (1.0 - pow(fFog, 0.8)) * 0.5;
	vec3 perturbTotal = perturbTotalFactor * (perturb1 + perturb2 + perturb3 + perturb4 + perturb5);
	perturbTotal.y = 0;
	normal = normalize(normal + perturbTotal);

	vec3 eyeDir = normalize(eyePos - fWPos);

	vec3 reflectDir = reflect(-eyeDir, normal);
	vec4 reflectColor = texture(textureReflection, reflectDir);

	vec3 reflectTint = vec3(0.5, 0.6, 0.65) * 1.2;
	reflectColor.xyz = pow(reflectColor.xyz, vec3(0.75));
	reflectColor.xyz *= reflectTint;

	vec3 fogDir = -eyeDir;
	fogDir.y = 0;
	vec4 fogColor = texture(textureReflection, fogDir);
	float fogFactor = pow(fFog, 0.50);

	float fresnelFactor = fresnel(1.0, 1.0, normal, -eyeDir);
	//vec3 ownColor = vec3(0.07, 0.16, 0.2);
	//vec3 color = mix(ownColor, reflectColor.xyz, fresnelFactor);
	
	vec3 color = mix(reflectColor.xyz, fogColor.xyz, fogFactor);
	
	float alpha = max(0.10, fresnelFactor);

	vec4 final = vec4(color, alpha);

	// DEBUG:
	float f = 1.0;
	final = vec4(f, f, f, 1.0) + 0.00001 * final;
	//final.a = 0.00001;

	gl_FragColor = final;
}

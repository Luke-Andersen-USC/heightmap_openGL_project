#version 150

in vec3 centerPosition;
in vec3 leftPosition;
in vec3 rightPosition;
in vec3 upPosition;
in vec3 downPosition;
out vec4 col;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

uniform float exponent;
uniform float scale;

void main()
{
  // compute the average position and adjust col / height
  vec3 averagePosition = (centerPosition + leftPosition + rightPosition + upPosition + downPosition) / 5;
  float yPow = pow(averagePosition.y, exponent);
  col = vec4(yPow, yPow, yPow, 1.0f);
  averagePosition.y = scale * yPow;

  // compute the transformed and projected vertex position (into gl_Position) 
  gl_Position = projectionMatrix * modelViewMatrix * vec4(averagePosition, 1.0f);
}


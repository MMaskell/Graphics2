#include "Mesh.h"

//Temp lighting
glm::vec3 ambientLight(0.2f, 0.2f, 0.2f);
glm::vec3 diffuseLight(1.0f, 1.0f, 1.0f);
glm::vec3 specularLight(0.2f, 0.2f, 0.2f);

glm::vec3 position(0.0f, 0.0f, 0.0f);
glm::vec3 direction(0.0f, 0.0f, 1.0f);

Mesh::Mesh() {
	shader = Shader("shaders/multiLight.vert", "shaders/multiLight.frag");
	program = shader.getProgram();
	glGenVertexArrays(1, &vertexArray);
	glGenBuffers(1, &elementBuffer);
	glGenBuffers(1, &vertexBuffer);
	glGenBuffers(1, &uvBuffer);
	glGenBuffers(1, &normalBuffer);
	glGenBuffers(1, &tangentBuffer);
	glGenBuffers(1, &bitangentBuffer);
	glUseProgram(program);
	glUniform1i(glGetUniformLocation(program, "diffuse"), 0);
	glUniform1i(glGetUniformLocation(program, "specular"), 1);
	glUniform1i(glGetUniformLocation(program, "normalMap"), 2);
	glUniform1i(glGetUniformLocation(program, "emissionMap"), 3);
	glUseProgram(0);
	shininess = 0;
}

//TODO: Copy

Mesh::~Mesh() {
	glDeleteBuffers(1, &elementBuffer);
	glDeleteBuffers(1, &vertexBuffer);
	glDeleteBuffers(1, &uvBuffer);
	glDeleteBuffers(1, &normalBuffer);
	glDeleteBuffers(1, &tangentBuffer);
	glDeleteBuffers(1, &bitangentBuffer);
	glDeleteVertexArrays(1, &vertexArray);
}

void Mesh::setMesh(vector<unsigned short> indices, vector<glm::vec3> vertices, vector<glm::vec2> uvs, vector<glm::vec3> normals, vector<glm::vec3> tangents, vector<glm::vec3> bitangents) {
	this->indices = indices;
	this->vertices = vertices;
	this->uvs = uvs;
	this->normals = normals;
	this->tangents = tangents;
	this->bitangents = bitangents;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &(this->indices[0]), GL_STATIC_DRAW);
	glBindVertexArray(vertexArray);
	//Pass vertices
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &(this->vertices[0]), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	//Pass UVs
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uvBuffer);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &(this->uvs[0]), GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	//Pass normals
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &(this->normals[0]), GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	//Pass tangents
	glEnableVertexAttribArray(3);
	glBindBuffer(GL_ARRAY_BUFFER, tangentBuffer);
	glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(glm::vec3), &(this->tangents[0]), GL_STATIC_DRAW);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	//Pass bitangents
	glEnableVertexAttribArray(4);
	glBindBuffer(GL_ARRAY_BUFFER, bitangentBuffer);
	glBufferData(GL_ARRAY_BUFFER, bitangents.size() * sizeof(glm::vec3), &(this->bitangents[0]), GL_STATIC_DRAW);
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
}

void Mesh::render(Camera* cam) {
	//Use correct shaders
	glUseProgram(program);
	//Enable the VAO
	glBindVertexArray(vertexArray);
	//Pass texture to shaders
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, diffuse);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, specular);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, normal);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, emission);
	//Shininess
	glUniform1f(glGetUniformLocation(program, "shininess"), shininess);
	//Pass matrices to shader
	glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, false, &(this->getGlobalMatrix())[0][0]);
	glm::mat3 inv = glm::mat3(glm::transpose(glm::inverse(this->getGlobalMatrix())));
	glUniformMatrix3fv(glGetUniformLocation(program, "transInvModel"), 1, false, &inv[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, false, &(cam->getView())[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, false, &(cam->getProjection())[0][0]);
	//Pass the camera position
	glUniform3fv(glGetUniformLocation(program, "viewPos"), 1, &(cam->getGlobalPosition())[0]);
	//Pass the MVP matrix to the shaders
	//glUniformMatrix4fv(mvpUniform, 1, false, &mvp[0][0]);
	//Draw the VAO
	//glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, 0);
}

void Mesh::setShininess(float shininess) {
	this->shininess = shininess;
}

void Mesh::setDiffuse(GLuint diffuse) {
	this->diffuse = diffuse;
}

void Mesh::setSpecular(GLuint specular) {
	this->specular = specular;
}

void Mesh::setEmission(GLuint emission) {
	this->emission = emission;
}

void Mesh::setNormal(GLuint normal) {
	this->normal = normal;
}

void Mesh::setName(string name) {
	this->name = name;
}

void Mesh::setModel(Model* m) {
	this->model = m;
}

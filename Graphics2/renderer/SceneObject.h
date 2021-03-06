#pragma once
/*
Scene Objects are any object that exists within a scene.
For example these can be:
-Objects with a physical location
-Objects that can be rendered (Renderables)
-The scene itself, technically
-Grouping objects
*/
#include <unordered_set>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
using std::unordered_set;
class Scene;

class SceneObject {
public:
	SceneObject();
	SceneObject(SceneObject& other);
	virtual ~SceneObject();
	//Sets the object's parent to obj,
	bool setParent(Scene* obj);
	//Sets the object's parent to obj, making it a part of that object's scene
	bool setParent(SceneObject* obj);
	//Gets the parent of the object
	SceneObject* getParent() { return parent; };
	//Gets the scene the object occupies
	Scene* getScene() { return scene; };
	//Gets the children of the object
	const unordered_set<SceneObject*>& getChildren() const { return children; };
	//Checks if the child is a direct child of the object
	bool hasChild(SceneObject* child);
	//Checks if the child is a descendent of the object
	bool hasDescendent(SceneObject* child);
	//Sets the local transformation matrix
	void setLocalMatrix(glm::mat4 local);
	//Gets the transformation matrix local to this object
	glm::mat4 getLocalMatrix();
	//Gets the transformation matrix in relation to the scene
	glm::mat4 getGlobalMatrix();
	//Sets the position of the object
	void setPosition(glm::vec3 pos);
	//Sets the scale of the object
	void setScale(glm::vec3 scale);
	//Sets the rotation of the object
	void setRotation(glm::quat rot);
	//Gets the postion of the object
	glm::vec3 getPosition();
	//Gets the scale of the object
	glm::vec3 getScale();
	//Gets the rotation of the object
	glm::quat getRotation();
	//Gets the cardinal directions under local transformation
	glm::vec3 getFront();
	glm::vec3 getUp();
	glm::vec3 getRight();
	//Gets the global position of the object
	glm::vec3 getGlobalPosition();

protected:
	virtual void setScene(Scene* s);
private:
	Scene* scene;
	SceneObject* parent;
	unordered_set<SceneObject*> children;
	glm::vec3 pos;
	glm::vec3 scale;
	glm::quat rot;
	glm::mat4 localMat;
	glm::mat4 globalMat;
	void updateMatrix();
};


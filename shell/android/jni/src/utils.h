#ifndef UTILS_H_
#define UTILS_H_

void setAPK (const char* apkPath);
//Filename will be looked up in the apk (should start with assets/ or res/
GLuint loadTextureFromPNG (const char* filename, int &width, int &height);

#endif /* UTILS_H_ */

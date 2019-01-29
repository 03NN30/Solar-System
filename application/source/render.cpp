#include "glewInc.hpp"
#include "render.hpp"
#include "sceneGraph.hpp"
#include "gui.hpp"

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>

Node* sg = SceneGraph::get().getRoot();

Shader sunShader("sun"); 
Shader planetShader("planet"); 
Shader earthShader("earth");  
Shader orbitShader("orbit");
Shader starShader("stars");
Shader skyboxShader("skybox");
Shader quadShader("quad");
Shader blurShader("blur");
Shader bloomShader("bloom");
Shader sunBloomShader("sunBloom");

Model sphere("sphere.obj");
Model cube("skybox.obj");
Model quad("quad.obj");

modelObject orbitModel;
modelObject starModel;
modelObject skyboxModel;

Skybox skybox;

float shininess        = 11.9f;
float ambient          = 0.04;
float lightIntensity   = 1.45f;
float reflectivity     = 0.137f;
float lightConstant    = 1.0f;
float lightLinear      = 0.001f;
float lightQuadratic   = 0.001f;
float exposure         = 2.01f;
float gamma            = 0.67f;
float glow             = 2.0;
  int bloom            = 1;
 bool planetOutline    = false;
 bool blur             = false;
 bool grayscale        = false;
 bool verticalMirror   = false;
 bool horizontalMirror = false;
 bool orbits           = true;
 bool stars            = true;
 bool realism          = false;

unsigned int quadVAO = 0;
unsigned int quadVBO;
unsigned int hdrFBO;
unsigned int pingpongFBO[2];
unsigned int colorBuffers[2];
unsigned int pingpongColorbuffers[2];
unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
unsigned int rboDepth;


void setupFramebuffer() {
    
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // create 2 floating point color buffers (1 for normal rendering, other for brightness treshold values)
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // create and attach depth buffer (renderbuffer)
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth, screenHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }
    
    //blurShader.use();
    //blurShader.setInt("image", 0);
    //bloomShader.use();
    //bloomShader.setInt("scene", 0);
    //bloomShader.setInt("bloomBlur", 1);
}

void setup() {
    sunShader.createShader();
    planetShader.createShader();
    orbitShader.createShader();
    starShader.createShader();
    earthShader.createShader();
    skyboxShader.createShader();
    quadShader.createShader();
    blurShader.createShader();
    bloomShader.createShader();
    sunBloomShader.createShader();
    
    sphere.setGeometry(GL_TRIANGLES); 
    cube.setGeometry(GL_TRIANGLES); 
    quad.setGeometry(GL_TRIANGLES);

    sg->setName("root");

    Node* sun     = new Node("sun",      0.0f,   0.0f, 1.0f, 0.5f, "planets/2k_sun.jpg");              
    Node* mercury = new Node("mercury",  3.0f,  0.06f, 0.2f, 1.0f, "planets/2k_mercury.jpg");   
    Node* venus   = new Node("venus",    6.0f,  0.05f, 0.2f, 1.0f, "planets/2k_venus.jpg");         
    Node* earth   = new Node("earth",    9.0f, 0.038f, 0.3f, 2.4f, "planets/2k_earth.jpg,planets/2k_earth_clouds.jpg,planets/2k_earth_nightmap.jpg");      
    Node* mars    = new Node("mars",    12.0f, 0.029f, 0.1f, 1.0f, "planets/2k_mars.jpg");          
    Node* jupiter = new Node("jupiter", 15.0f, 0.022f, 0.7f, 1.0f, "planets/2k_jupiter.jpg");       
    Node* saturn  = new Node("saturn",  18.0f, 0.034f, 0.6f, 1.0f, "planets/2k_saturn.jpg");        
    Node* uranus  = new Node("uranus",  21.0f, 0.026f, 0.4f, 1.0f, "planets/2k_uranus.jpg");        
    Node* neptune = new Node("neptune", 24.0f, 0.028f, 0.4f, 1.0f, "planets/2k_neptune.jpg");       
    Node* pluto   = new Node("pluto",   27.0f, 0.031f, 0.1f, 1.0f, "planets/plutomap.png");         
    Node* moon    = new Node("moon",     2.0f,   1.0f, 0.1f, 2.0f, "planets/2k_moon.jpg");   
    
    sun->setName("sun");            sg->addChild(sun);                      
    mercury->setName("mercury");    sg->addChild(mercury);                  
    venus->setName("venus");        sg->addChild(venus);                    
    earth->setName("earth");        sg->addChild(earth);                    
    mars->setName("mars");          sg->addChild(mars);                     
    jupiter->setName("jupiter");    sg->addChild(jupiter);                  
    saturn->setName("saturn");      sg->addChild(saturn);                   
    uranus->setName("uranus");      sg->addChild(uranus);                   
    neptune->setName("neptune");    sg->addChild(neptune);                  
    pluto->setName("pluto");        sg->addChild(pluto);                    
    moon->setName("moon");          sg->getChild("earth")->addChild(moon);  

    skybox.setPaths("milkyway/XP.jpg", "milkyway/XN.jpg", "milkyway/YP.jpg", "milkyway/YN.jpg", "milkyway/ZP.jpg", "milkyway/ZN.jpg");

    initializeSkybox();
    initializeOrbits();
    initializeStars(10000);

    setupFramebuffer();
    //Framebuffer::get();
}

void update() {
    uploadView();
    uploadProjection();
}

void render() {
 
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);   

    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    //Framebuffer::get().bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (stars) {
            drawStars();   
        }

        recursRender(*sg);

        glDepthFunc(GL_LEQUAL);
        drawSkybox();  
        glDepthFunc(GL_LESS);  
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //Framebuffer::get().unbind(); 

    drawFramebuffer(); 
}

void recursRender(Node& it, glm::fmat4& mat) {    

    if (it.getVisibility()) {
        if (it.getName() != sg->getName()) {
            it.setWorldTransform(mat);
            
            if (it.getName() == "sun") {
                drawSun(it);
            } else {
                if (it.getName() == "earth") {
                    drawEarth(it);
                } else {
                    drawPlanet(it);  
                }
                if (orbits) {
                    drawOrbit(it, mat);
                }
            }
        }

        if (!it.getChildrenList().empty()) {

            for (Node* itChild : it.getChildrenList()) {
                Node ptr = *itChild;
                recursRender(*itChild, it.getWorldTransform());
            }
        }
    }
}

void initializeStars(unsigned int amount) {
    std::vector<GLfloat> starVec;
    for (unsigned int i = 0; i < amount; ++i) {
        starVec.push_back(static_cast<GLfloat> (rand() % 200 + 1) - 100);
    }

    glGenVertexArrays(1, &starModel.VAO);
    glBindVertexArray(starModel.VAO);

    glGenBuffers(1, &starModel.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, starModel.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * starVec.size(), starVec.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_TRUE, 3 * sizeof(float), (void*)0);

    glGenBuffers(1, &starModel.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, starModel.EBO);

    starModel.draw_mode = GL_POINTS;
    starModel.num_elements = GLsizei(starVec.size());
}

void initializeOrbits() {
    std::vector<GLfloat> orbitVec;

    for (unsigned int i = 0; i < 361; i++){
        orbitVec.push_back(static_cast<GLfloat>(cos((i * M_PI)/180))); 
        orbitVec.push_back(static_cast<GLfloat>(-sin((i * M_PI)/180)));
    }
    
    glGenVertexArrays(1, &orbitModel.VAO);
    glBindVertexArray(orbitModel.VAO);

    glGenBuffers(1, &orbitModel.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, orbitModel.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * orbitVec.size(), orbitVec.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)(1 * sizeof(float)));
    
    glGenBuffers(1, &orbitModel.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, orbitModel.EBO);

    orbitModel.draw_mode = GL_LINE_LOOP;
    orbitModel.num_elements = GLsizei(orbitVec.size() / 2);
}

void initializeSkybox() {
    float skyboxVertices[] = {        
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &skyboxModel.VAO);
    glGenBuffers(1, &skyboxModel.VBO);
    glBindVertexArray(skyboxModel.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxModel.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    skyboxModel.draw_mode = GL_TRIANGLES;
    skyboxModel.num_elements = GLsizei(36);
}

void drawFramebuffer() {
    blurShader.use();
    blurShader.setInt("image", 0);
    bloomShader.use();

    bloomShader.setInt("scene", 0);
    bloomShader.setInt("bloomBlur", 1);

    bool horizontal = true, first_iteration = true;
    unsigned int amount = 10;
    blurShader.use();
    
    for (unsigned int i = 0; i < amount; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
        //glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer::get().getpingpongFBO(horizontal));
        
        blurShader.setInt("horizontal", horizontal);

        glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]); 
        //glBindTexture(GL_TEXTURE_2D, first_iteration ? Framebuffer::get().getcolorBuffers(1) : Framebuffer::get().getpingpongColorBuffers(!horizontal)); 

        renderQuad();
        horizontal = !horizontal;
        if (first_iteration)
            first_iteration = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    bloomShader.use();
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
    //glBindTexture(GL_TEXTURE_2D, Framebuffer::get().getcolorBuffers(0));

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]); 
    //glBindTexture(GL_TEXTURE_2D, Framebuffer::get().getpingpongColorBuffers(!horizontal));

    bloomShader.setFloat("gamma", gamma);
    bloomShader.setBool("blur", blur);
    bloomShader.setBool("grayscale", grayscale);
    bloomShader.setBool("verticalMirror", verticalMirror);
    bloomShader.setBool("horizontalMirror", horizontalMirror);
    bloomShader.setFloat("exposure", exposure);
    renderQuad();
}

void renderQuad() {
    quad.setVertexAttributes();
    quad.draw();
}

void drawQuad() {
    quadShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Framebuffer::get().getTextureID());
    quadShader.setInt("texture1", 0);

    quadShader.setBool("blur", blur);
    quadShader.setBool("grayscale", grayscale);
    quadShader.setBool("verticalMirror", verticalMirror);
    quadShader.setBool("horizontalMirror", horizontalMirror);
    quadShader.setFloat("exposure", exposure);
    quadShader.setFloat("gamma", gamma);

    quad.setVertexAttributes();
    quad.draw();
}

void drawOrbit(Node& it, glm::fmat4 &mat) {
    orbitShader.use();

    glm::vec3 origin;
    glm::fmat4 root;

    glm::fmat4 model_matrix = mat;

    root = it.getParent()->getLocalTransform();
    
    origin[0] = root[3][0];
    origin[1] = root[3][1];
    origin[2] = root[3][2];

    glm::vec3 scale_dir(it.getDistanceFromOrigin(), it.getDistanceFromOrigin(), it.getDistanceFromOrigin());
    model_matrix = glm::translate(model_matrix, origin);
    model_matrix = glm::scale(model_matrix, scale_dir);
    
    orbitShader.setModel(model_matrix);
    
    glBindVertexArray(orbitModel.VAO);
    glDrawArrays(orbitModel.draw_mode, 0, orbitModel.num_elements); 
    glBindVertexArray(0);
}

void drawStars() {
    starShader.use();
    starShader.setModel();
    glBindVertexArray(starModel.VAO);
    glDrawArrays(starModel.draw_mode, 0, starModel.num_elements);
}

void drawPlanet(Node& it) {
    planetShader.use();

    if (it.getTexturePath() != "") {     
        planetShader.setInt("texture1", 0);
        glActiveTexture(GL_TEXTURE0);
        it.getTextureList().at(0)->bind();
    }

    glm::fmat4 model_matrix = it.getWorldTransform();
    planetShader.setModel(model_matrix);
    planetShader.setfVec3("LightPosition", sg->getLocalTransform()[3][0], sg->getLocalTransform()[3][1], sg->getLocalTransform()[3][2]);
    planetShader.setfVec3("LightColor", 0.0f, 0.0f, 0.0f);
    planetShader.setFloat("Shininess", shininess);
    planetShader.setFloat("AmbientVal", ambient);
    planetShader.setFloat("LightIntensity", lightIntensity);
    planetShader.setFloat("Reflectivity", reflectivity);    
    planetShader.setBool("outline", planetOutline);
    planetShader.setFloat("LightConstant", lightConstant);
    planetShader.setFloat("LightLinear", lightLinear);
    planetShader.setFloat("LightQuadratic", lightQuadratic);
    planetShader.setfVec3("viewPos", Camera::get().position);

    sphere.setVertexAttributes();
    sphere.draw();    
}

void drawEarth(Node& it) {
    earthShader.use();
    
    if (it.getTexturePath() != "") { 
        switch (it.getTextureList().size()) {
        case 1: 
            earthShader.setInt("texture1", 0);
            glActiveTexture(GL_TEXTURE0);
            it.getTextureList().at(0)->bind();
        break;
        case 2:
            earthShader.setInt("texture1", 0);
            earthShader.setInt("texture2", 1);
            glActiveTexture(GL_TEXTURE0);
            it.getTextureList().at(0)->bind();
            glActiveTexture(GL_TEXTURE1);
            it.getTextureList().at(1)->bind();
        break;
        case 3:
            earthShader.setInt("texture1", 0);
            earthShader.setInt("texture2", 1);
            earthShader.setInt("texture3", 2);
            glActiveTexture(GL_TEXTURE0);
            it.getTextureList().at(0)->bind();
            glActiveTexture(GL_TEXTURE1);
            it.getTextureList().at(1)->bind();
            glActiveTexture(GL_TEXTURE2);
            it.getTextureList().at(2)->bind();
        }
    }
    
    glm::fmat4 model_matrix = it.getWorldTransform();
    earthShader.setModel(model_matrix);
    earthShader.setfVec3("LightPosition", sg->getLocalTransform()[3][0], sg->getLocalTransform()[3][1], sg->getLocalTransform()[3][2]);
    earthShader.setfVec3("LightColor", 0.0f, 0.0f, 0.0f);
    earthShader.setFloat("Shininess", shininess);
    earthShader.setFloat("AmbientVal", ambient);
    earthShader.setFloat("LightIntensity", lightIntensity);
    earthShader.setFloat("Reflectivity", reflectivity);    
    earthShader.setBool("outline", planetOutline);
    earthShader.setFloat("LightConstant", lightConstant);
    earthShader.setFloat("LightLinear", lightLinear);
    earthShader.setFloat("LightQuadratic", lightQuadratic);
    earthShader.setfVec3("viewPos", Camera::get().position);

    if (realism) {
        quad.setVertexAttributes();
        quad.draw(); 
    } else {
        sphere.setVertexAttributes();
        sphere.draw();  
    }          
}

void drawSun(Node& it) {
    glm::fmat4 model_matrix = it.getWorldTransform();

    sunBloomShader.use();
    sunBloomShader.setfMat4("model", model_matrix);
    sunBloomShader.setFloat("glow", glow);

    if (it.getTexturePath() != "") {
        sunBloomShader.setInt("texture1", 0);
        glActiveTexture(GL_TEXTURE0);
        it.getTextureList().at(0)->bind();
    } else {
        std::cerr << "ERROR::TEXTURE::NOT DEFINED FOR \"" << it.getName() << "\"" << std::endl;
    }
    sphere.setVertexAttributes();
    sphere.draw();
    
}

void drawSkybox() {
    skyboxShader.use();

    skyboxShader.setInt("skybox", 0);
    skybox.bind();

    glm::fmat4 model_matrix = glm::fmat4(1.0f);
    model_matrix = glm::translate(model_matrix, Camera::get().position);
    model_matrix = glm::scale(model_matrix, glm::vec3(60.0f, 60.0f, 60.0f));
    skyboxShader.setModel(model_matrix);

    glBindVertexArray(skyboxModel.VAO);
    glDrawArrays(skyboxModel.draw_mode, 0, skyboxModel.num_elements);
    glBindVertexArray(0);
}

void uploadView() {
    glm::mat4 view = Camera::get().getViewMatrix();
    sunShader.setView(view);
    earthShader.setView(view);
    planetShader.setView(view);
    starShader.setView(view);
    skyboxShader.setView(view);
    orbitShader.setView(view);
    sunBloomShader.setView(view);
}

void uploadProjection() {
    glm::mat4 projection = Camera::get().getProjectionMatrix();
    sunShader.setProjection(projection);
    earthShader.setProjection(projection);
    planetShader.setProjection(projection);
    starShader.setProjection(projection);
    skyboxShader.setProjection(projection);
    orbitShader.setProjection(projection);
    sunBloomShader.setProjection(projection);
}


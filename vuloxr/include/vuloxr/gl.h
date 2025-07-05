#pragma once

namespace vuloxr {

namespace gl {

struct ShaderCompile {
  uint32_t id;
  ShaderCompile(int shaderType, std::span<const char *> srcs) {
    this->id = glCreateShader(shaderType);
    glShaderSource(this->id, srcs.size(), srcs.data(), NULL);
    glCompileShader(this->id);
  }
  ~ShaderCompile() {}
  bool check() const {
    GLint stat;
    glGetShaderiv(this->id, GL_COMPILE_STATUS, &stat);
    if (!stat) {
      GLsizei len;
      char *lpBuf;

      glGetShaderiv(this->id, GL_INFO_LOG_LENGTH, &len);
      lpBuf = (char *)malloc(len);

      glGetShaderInfoLog(this->id, len, &len, lpBuf);
      Logger::Error("Error: problem compiling shader.\n");
      Logger::Error("-----------------------------------\n");
      Logger::Error("%s\n", lpBuf);
      Logger::Error("-----------------------------------\n");

      free(lpBuf);

      return false;
    }
    return true;
  }
};

struct ShaderProgram : vuloxr::NonCopyable {
  uint32_t id;
  ShaderProgram() { this->id = glCreateProgram(); }
  bool link(uint32_t vs, uint32_t fs) {
    glAttachShader(this->id, vs);
    glAttachShader(this->id, fs);
    glLinkProgram(this->id);

    GLint stat;
    glGetProgramiv(this->id, GL_LINK_STATUS, &stat);
    if (!stat) {
      GLsizei len;
      char *lpBuf;

      glGetProgramiv(this->id, GL_INFO_LOG_LENGTH, &len);
      lpBuf = (char *)malloc(len);

      glGetProgramInfoLog(this->id, len, &len, lpBuf);
      Logger::Error("Error: problem linking shader.\n");
      Logger::Error("-----------------------------------\n");
      Logger::Error("%s\n", lpBuf);
      Logger::Error("-----------------------------------\n");

      free(lpBuf);

      return false;
    }
    return true;
  }
  ~ShaderProgram() {}
  ShaderProgram(ShaderProgram &&rhs) {
    this->id = rhs.id;
    rhs.id = 0;
  }
  ShaderProgram &operator=(ShaderProgram &&rhs) {
    this->id = rhs.id;
    rhs.id = 0;
    return *this;
  }
  bool compile(std::span<const char *> vsSrcs, std::span<const char *> fsSrc) {
    ShaderCompile vs(GL_VERTEX_SHADER, vsSrcs);
    if (!vs.check()) {
      return false;
    }
    ShaderCompile fs(GL_FRAGMENT_SHADER, fsSrc);
    if (!fs.check()) {
      return false;
    }
    return link(vs.id, fs.id);
  }
  void bind() { glUseProgram(this->id); }
  void unbind() { glUseProgram(0); }
};

struct DepthTexture : vuloxr::NonCopyable {
  uint32_t id;
  DepthTexture(int width, int height) {
    glGenRenderbuffers(1, &this->id);
    bind();
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    unbind();
  }
  void bind() { glBindRenderbuffer(GL_RENDERBUFFER, this->id); }
  void unbind() { glBindRenderbuffer(GL_RENDERBUFFER, 0); }
};

struct FramebufferObject : vuloxr::NonCopyable {
  uint32_t id;
  FramebufferObject() { glGenFramebuffers(1, &this->id); }
  ~FramebufferObject() {}
  void bind() { glBindFramebuffer(GL_FRAMEBUFFER, this->id); }
  void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
  void attach(uint32_t color_id, uint32_t depth_id) {
    bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           color_id, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depth_id);
    GLenum stat = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(stat == GL_FRAMEBUFFER_COMPLETE);
    unbind();
  }
};

struct RenderTarget : vuloxr::NonCopyable {
  DepthTexture depth;
  FramebufferObject fbo;

  RenderTarget(uint32_t image_id, int width, int height)
      : depth(width, height) {
    fbo.attach(image_id, this->depth.id);
    vuloxr::Logger::Info("SwapchainImage FBO:%d, TEXC:%d, TEXZ:%d, WH(%d, %d)",
                         this->fbo.id, image_id, this->depth.id, width, height);
  }

  void beginFrame(int width, int height, const XrColor4f &clearColor) {
    // tell GL to only draw onto a pixel if the shape is closer to the viewer
    glEnable(GL_DEPTH_TEST); // enable depth-testing
    glDepthFunc(
        GL_LESS); // depth-testing interprets a smaller value as "closer"

    this->fbo.bind();
    glViewport(0, 0, width, height);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  void endFrame() { this->fbo.unbind(); }
};

struct Vbo {
  uint32_t id;
  uint32_t drawCount = 0;
  Vbo() { glGenBuffers(1, &this->id); }
  ~Vbo() {}

  void bind() { glBindBuffer(GL_ARRAY_BUFFER, this->id); }
  void unbind() { glBindBuffer(GL_ARRAY_BUFFER, 0); }
  void assign(const void *p, uint32_t size, uint32_t drawCount) {
    bind();
    glBufferData(GL_ARRAY_BUFFER, size, p, GL_STATIC_DRAW);
    unbind();
    this->drawCount = drawCount;
  }
  template <typename T> void assign(std::span<const T> data) {
    assign(data.data(), sizeof(T) * data.size());
  }
  void draw() { glDrawArrays(GL_TRIANGLE_STRIP, 0, this->drawCount); }
};

struct Ibo {
  uint32_t id;
  uint32_t drawCount = 0;
  uint32_t drawType = GL_UNSIGNED_INT;
  Ibo() { glGenBuffers(1, &this->id); }
  ~Ibo() {}

  void bind() { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->id); }
  void unbind() { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }
  void assign(const void *p, uint32_t size, uint32_t drawCount) {
    bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, p, GL_STATIC_DRAW);
    unbind();
    this->drawCount = drawCount;
    switch (size / drawCount) {
    case 2:
      this->drawType = GL_UNSIGNED_SHORT;
      break;
    case 4:
      this->drawType = GL_UNSIGNED_INT;
      break;
    }
  }
  template <typename T> void assign(std::span<const T> data) {
    assign(data.data(), sizeof(T) * data.size(), data.size());
  }
  void draw() {
    glDrawElements(GL_TRIANGLES, this->drawCount, this->drawType, 0);
  }
};

struct Vao {
  struct AttributeLayout {
    int32_t attribute;
    uint32_t vbo;
    uint32_t components;
    uint32_t offset;
  };
  uint32_t id;
  Vao() { glGenVertexArrays(1, &this->id); }
  ~Vao() {}
  void bind() { glBindVertexArray(this->id); }
  void unbind() { glBindVertexArray(0); }
  void assign(uint32_t stride, std::span<const AttributeLayout> layouts) {
    bind();
    for (auto &layout : layouts) {
      glEnableVertexAttribArray(layout.attribute);
      glBindBuffer(GL_ARRAY_BUFFER, layout.vbo);
      glVertexAttribPointer(layout.attribute, layout.components, GL_FLOAT,
                            GL_FALSE, stride,
                            (const void *)(int64_t)layout.offset);
    }
    unbind();
    for (auto &layout : layouts) {
      glEnableVertexAttribArray(layout.attribute);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
  }
  void assign(uint32_t stride, std::span<const AttributeLayout> layouts,
              uint32_t ibo) {
    bind();
    for (auto &layout : layouts) {
      glEnableVertexAttribArray(layout.attribute);
      glBindBuffer(GL_ARRAY_BUFFER, layout.vbo);
      glVertexAttribPointer(layout.attribute, layout.components, GL_FLOAT,
                            GL_FALSE, stride,
                            (const void *)(int64_t)layout.offset);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    unbind();
    for (auto &layout : layouts) {
      glEnableVertexAttribArray(layout.attribute);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }
};

struct Ubo {
  uint32_t ubo_ = 0;

  static std::shared_ptr<Ubo> Create(uint32_t size, const void *data) {
    auto ptr = std::make_shared<Ubo>();

    glGenBuffers(1, &ptr->ubo_);
    glBindBuffer(GL_UNIFORM_BUFFER, ptr->ubo_);
    if (data) {
      glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STATIC_DRAW);
    } else {
      glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    return ptr;
  }
  template <typename T> static std::shared_ptr<Ubo> Create() {
    return Create(sizeof(T), nullptr);
  }

  void Bind() { glBindBuffer(GL_UNIFORM_BUFFER, ubo_); }
  void Unbind() { glBindBuffer(GL_UNIFORM_BUFFER, 0); }
  void Upload(uint32_t size, const void *data) {
    Bind();
    glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
    Unbind();
  }
  template <typename T> void Upload(const T &data) { Upload(sizeof(T), &data); }
  void SetBindingPoint(uint32_t binding_point) {
    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, ubo_);
  }
};

} // namespace gl

} // namespace vuloxr

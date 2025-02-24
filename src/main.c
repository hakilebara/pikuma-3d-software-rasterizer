#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "array.h"
#include "display.h"
#include "vector.h"
#include "mesh.h"

// Array of triangles that should be rendered frame by frame
triangle_t* triangles_to_render = NULL;

// Global variables for execution status and game loop
bool is_running = false;
int previous_frame_time = 0;

vec3_t camera_position = { 0, 0, 0 };
float fov_factor = 640;


// Setup function to initialize variables and game objects
void setup(void) {
  // Initialize render mode and triangle culling method
  render_method = RENDER_WIRE;
  cull_method = CULL_BACKFACE;

  // Allocate the required memory in bytes to hold the color buffer
  color_buffer = (uint32_t*) malloc(sizeof(uint32_t) * window_height *
      window_width);

  // Creating a SDL texture that is used to display the color buffer
  color_buffer_texture = SDL_CreateTexture(
      renderer,
      SDL_PIXELFORMAT_ARGB8888,
      SDL_TEXTUREACCESS_STREAMING,
      window_width,
      window_height
  );

  // Loads the cube values in the mesh data structure
  load_cube_mesh_data();
  // load_obj_file_data("./assets/cube.obj");
}

void process_input(void) {
  SDL_Event event;
  SDL_PollEvent(&event);

  switch (event.type) {
    case SDL_QUIT:
      is_running = false;
      break;
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE)
        is_running = false;
      if (event.key.keysym.sym == SDLK_1)
        render_method = RENDER_WIRE_VERTEX;
      if (event.key.keysym.sym == SDLK_2)
        render_method = RENDER_WIRE;
      if (event.key.keysym.sym == SDLK_3)
        render_method = RENDER_FILL_TRIANGLE;
      if (event.key.keysym.sym == SDLK_4)
        render_method = RENDER_FILL_TRIANGLE_WIRE;
      if (event.key.keysym.sym == SDLK_c)
        cull_method = CULL_BACKFACE;
      if (event.key.keysym.sym == SDLK_d)
        cull_method = CULL_NONE;
      break;
  }
}

// Function that receives a 3D vector and returns a projected 2D point
vec2_t project(vec3_t point) {
  vec2_t projected_point = {
    .x = (fov_factor * point.x) / point.z,
    .y = (fov_factor * point.y) / point.z
  };
  return projected_point;
}

// Update function frame by frame with a fixed time step
void update(void) {
  // Wait some time until we reach the target frame time in milliseconds
  int time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() -
      previous_frame_time);

  // only delay execution if we are running too fast
  if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
    SDL_Delay(time_to_wait);
  }

  previous_frame_time = SDL_GetTicks();

  // Initialize the array of triangles to render]
  // Every update we are going to reset
  triangles_to_render = NULL;


  mesh.rotation.x += 0.01;
  mesh.rotation.y += 0.01;
  mesh.rotation.z += 0.02;

  // loop all triangle faces of our mesh
  int num_faces = array_length(mesh.faces);
  for (int i = 0; i < num_faces; ++i) {
    face_t mesh_face = mesh.faces[i];

    vec3_t face_vertices[3];
    face_vertices[0] = mesh.vertices[mesh_face.a - 1];
    face_vertices[1] = mesh.vertices[mesh_face.b - 1];
    face_vertices[2] = mesh.vertices[mesh_face.c - 1];

    vec3_t transformed_vertices[3];
    
    // loop all three vertices of this current face and apply transformations
    for (int j = 0; j < 3; ++j) {
      vec3_t transformed_vertex = face_vertices[j];

      transformed_vertex = vec3_rotate_x(transformed_vertex, mesh.rotation.x);
      transformed_vertex = vec3_rotate_y(transformed_vertex, mesh.rotation.y);
      transformed_vertex = vec3_rotate_z(transformed_vertex, mesh.rotation.z);

      // Translate the vertex away from the camera
      transformed_vertex.z += 5;

      // Save the transformed vertex in the array of transformed vertices
      transformed_vertices[j] = transformed_vertex;
    }

    // Check backface culling
    if (cull_method == CULL_BACKFACE) {
      vec3_t vertex_a = transformed_vertices[0]; /*   A   */
      vec3_t vertex_b = transformed_vertices[1]; /*  / \  */
      vec3_t vertex_c = transformed_vertices[2]; /* B---C */

      vec3_t vector_ab = vec3_sub(vertex_b, vertex_a);
      vec3_t vector_ac = vec3_sub(vertex_c, vertex_a);
      vec3_normalize(&vector_ab);
      vec3_normalize(&vector_ac);

      // Compute the face normal (using cross product to find perpendicular)
      vec3_t normal = vec3_cross(vector_ab, vector_ac);

      // Normalize the face normal vector
      vec3_normalize(&normal);

      // Find the vector between a point in the triangle and the camera origin
      vec3_t camera_ray = vec3_sub(camera_position, vertex_a);

      // Calculate how aligned the cameray is to the normal using dot product
      float dot_normal_camera = vec3_dot(normal, camera_ray);

      // Bypass the triangles that are looking away from the camera
      if (dot_normal_camera < 0 ) continue;
    }


    vec2_t projected_points[3];

    // Loop  all three vertices to perform the projection
    for (int j = 0; j < 3; ++j) {
      // Project the current vortex
      projected_points[j] = project(transformed_vertices[j]);

      // Scale and translate the projected point to the middle of the screen
      projected_points[j].x += (window_width   / 2);
      projected_points[j].y += (window_height / 2);
    }

    // Calculate the avg depth for each face baserd on the vertices after
    // transformation
    float avg_depth = (transformed_vertices[0].z + transformed_vertices[1].z + transformed_vertices[2].z) / 3.0;

    triangle_t projected_triangle = {
      .points = {
        { projected_points[0].x, projected_points[0].y },
        { projected_points[1].x, projected_points[1].y },
        { projected_points[2].x, projected_points[2].y }
      },
      .color = mesh_face.color,
      .avg_depth = avg_depth
    };

    // Save the projected triangle in the array of triangles to render
    array_push(triangles_to_render, projected_triangle);
  }

  // Sort the triangles to render by their avg_depth
  int num_triangles = array_length(triangles_to_render);
  for (int i = 0; i < num_triangles; ++i) {
    for (int j = i; j < num_triangles; ++j) {
      if (triangles_to_render[i].avg_depth < triangles_to_render[j].avg_depth) {
        triangle_t temp = triangles_to_render[i];
        triangles_to_render[i] = triangles_to_render[j];
        triangles_to_render[j] = temp;
      }
    }
  }
}

void render(void) {
  draw_grid();

  // loop al projected triangles and render them
  int num_triangles = array_length(triangles_to_render);
  for (int i = 0; i < num_triangles; ++i) {
    triangle_t triangle = triangles_to_render[i];


    // Draw filled triangle 
    if (render_method == RENDER_FILL_TRIANGLE || render_method ==
        RENDER_FILL_TRIANGLE_WIRE) {
      draw_filled_triangle(
          triangle.points[0].x, triangle.points[0].y, // vertex A
          triangle.points[1].x, triangle.points[1].y, // vertex B
          triangle.points[2].x, triangle.points[2].y, // vertex C
          triangle.color
      );
    }

    // Draw triangle wireframe
    if (render_method == RENDER_WIRE || render_method == RENDER_WIRE_VERTEX ||
        render_method == RENDER_FILL_TRIANGLE_WIRE) {
      draw_triangle(
          triangle.points[0].x, triangle.points[0].y, // vertex A
          triangle.points[1].x, triangle.points[1].y, // vertex B
          triangle.points[2].x, triangle.points[2].y, // vertex C
          0xFFFFFFFF
      );
    }

    // Draw vertex points
    if (render_method == RENDER_WIRE_VERTEX) {
      draw_rect(triangle.points[0].x - 3, triangle.points[0].y - 3, 6, 6, 0xFF00FF00);
      draw_rect(triangle.points[1].x - 3, triangle.points[1].y - 3, 6, 6, 0xFF00FF00);
      draw_rect(triangle.points[2].x - 3, triangle.points[2].y - 3, 6, 6, 0xFF00FF00);
    }
  }

  // Clear the array of triangle to render every frame loop
  array_free(triangles_to_render);

  render_color_buffer();
  clear_color_buffer(0xFF000000);

  SDL_RenderPresent(renderer);
}

// Free the memory that was dynamically allocated by the program
void free_resources(void)
{
  free(color_buffer);
  array_free(mesh.vertices);
  array_free(mesh.faces);
}

int main(void) {
  is_running = initialize_window();

  setup();

  while(is_running) {
    process_input();
    update();
    render();
  }

  destroy_window();
  free_resources();
  return 0;
}

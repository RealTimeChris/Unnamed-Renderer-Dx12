# Unnamed Renderer: 3-Stage Compute Path-Tracer
Implemented using DirectX 12 Compute shaders. Inspired by Peter Shirley's Ray Tracing in One Weekend.

## Materials
-Lambertian

-Metallic

-Dielectric

-Diffuse Light

## Primitives
-Procedural Spheres

-Procedural Rectangles

-Procedural Triangles
## Camera
Currently just a basic camera that is fixed right near the world origin, with modifiable field-of-view, where the view port is defined as:

-1.0f <= y <= +1.0f and,
(-1.0f * AspectRatio) <= x <= (+1.0f * AspectRatio)

## Rendering Method
Render-Pass 1: Generates an Intersection Map which is a collection of recorded intersections/misses between the Camera Paths and the Scene Geometry.

Render-Pass 2: Consumes the Intersection Map to generate an Accumulation Frame, which is the total summed Samples for each Pixel.

Render-Pass 3: Consumes the Accumulation Frame to produce the Final Frame, which is then copied into Host RAM for later presentation.
## 3000SPP, 30B, 720p
![](https://github.com/RealTimeChris/Unnamed-Renderer-DX12/blob/main/Images/124,%203000SPP,%2030B,%20720p.png?raw=true)
## 3000SPP, 30B, 720p
![](https://github.com/RealTimeChris/Unnamed-Renderer-DX12/blob/main/Images/125,%203000SPP,%2030B,%20720p.png?raw=true)
## 3000SPP, 30B, 720p
![](https://github.com/RealTimeChris/Unnamed-Renderer-DX12/blob/main/Images/122,%203000SPP,%2030B,%20720p.png?raw=true)
## 3000SPP, 30B, 720p
![](https://github.com/RealTimeChris/Unnamed-Renderer-DX12/blob/main/Images/103,%203000SPP,%2030B,%20720p.png?raw=true)
## 3000SPP, 30B, 720p
![](https://github.com/RealTimeChris/Unnamed-Renderer-DX12/blob/main/Images/116,%203000SPP,%2030B,%20720p.png?raw=true)
## 720p, 1500SPP, 30B Caustics 01
![](https://github.com/RealTimeChris/Unnamed-Renderer-DX12/blob/main/Images/720p,%201500SPP,%2030B%20Caustics%2001.png?raw=true)
## 720p, 1000SPP, 20B
![](https://github.com/RealTimeChris/Unnamed-Renderer-DX12/blob/main/Images/720p,%201000SPP,%2020B%202020-04-16-01.png?raw=true)
## 720p, 200SPP, 7 Bounces
![](https://github.com/RealTimeChris/Unnamed-Renderer-DX12/blob/main/Images/720p%2C%20200SPP%2C%207%20Bounces%2C%2002.png?raw=true)

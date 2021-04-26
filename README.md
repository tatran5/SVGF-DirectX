# Spatiotemporal Variance-Guided Filtering - DirectX
University of Pennsylvania, CIS 497 Senior Design

## Table of contents
- Introduction
- SVGF pipeline
- Implementation
- Performance analysis
- Potential improvements

## Introduction
Rendering in real-time with ray tracing is a challenge due to the requirement of a noise-free render, yet with only few samples per pixel. To avoid spending a lot of time to accumulate samples and get a decent render, denoising methods are introduced to process renders with few samples per pixel and get a noise-free result. Spatiotemporal Variance-Guided Filtering with DirectX is one of the methods to do so. The method utilizes spatial data (from neighboring pixels) within the same frame and temporal data (from previous frames) to smooth out each render. 

## SVGF pipeline

## Implementation

## Performance analysis

### Visual 

#### Noise at the edge of the renders

#### 

### Runtime


## Potential improvements

#### Use rasterized G-buffer 
- Current G-buffers are obtained through ray-tracing, which can be slower than using rasterization. The runtime can be improved if the current pipeline is changed to hybrid rendering.

#### Buffer packing
- Since reading from and writing to buffers are expensive, the world position buffer can be packed into depth buffer and stored as the fourth element in the normal buffer. 

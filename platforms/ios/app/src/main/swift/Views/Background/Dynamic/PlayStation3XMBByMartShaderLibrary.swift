// PlayStation3XMBByMartShaderLibrary.swift — runtime Metal library for the XMB background
// SPDX-License-Identifier: MIT

import Foundation
@preconcurrency import Metal

@MainActor
enum PlayStation3XMBByMartShaderLibrary {
  private static var cachedLibrary: MTLLibrary?
  private static var activeRendererCount = 0

  static func makeLibrary(device: MTLDevice) -> MTLLibrary? {
    if let cachedLibrary {
      return cachedLibrary
    }

    if let defaultLibrary = device.makeDefaultLibrary(),
      requiredFunctions.allSatisfy({
        defaultLibrary.makeFunction(name: $0) != nil
      })
    {
      cachedLibrary = defaultLibrary
      return defaultLibrary
    }

    do {
      let library = try device.makeLibrary(source: source, options: nil)
      cachedLibrary = library
      return library
    } catch {
      NSLog(
        "[ARMSX2 Dynamic Backgrounds] XMB shader compilation failed: %@",
        error.localizedDescription
      )
      return nil
    }
  }

  static func retainForRenderer() {
    activeRendererCount += 1
  }

  static func releaseForRenderer() {
    guard activeRendererCount > 0 else {
      cachedLibrary = nil
      return
    }

    activeRendererCount -= 1
    if activeRendererCount == 0 {
      cachedLibrary = nil
    }
  }

  static func releaseIfUnused() {
    if activeRendererCount == 0 {
      cachedLibrary = nil
    }
  }

  private static let requiredFunctions = [
    "xmbBackgroundVertex",
    "xmbBackgroundFragment",
    "xmbWaveVertex",
    "xmbWaveFragment",
    "xmbParticleVertex",
    "xmbParticleFragment",
  ]

  private static let source = #"""
// PlayStation 3 XMB recreation shaders
// SPDX-License-Identifier: MIT

#include <metal_stdlib>
using namespace metal;

struct XMBBackgroundUniforms {
  float4 startColor;
  float4 endColor;
  float4 directionRange;
  float4 geometry;
};

struct XMBBackgroundVertexOut {
  float4 position [[position]];
  float2 uvYDown;
};

vertex XMBBackgroundVertexOut xmbBackgroundVertex(
  uint vertexID [[vertex_id]]
) {
  const float2 positions[4] = {
    float2(-1.0, -1.0),
    float2(1.0, -1.0),
    float2(-1.0, 1.0),
    float2(1.0, 1.0),
  };
  float2 position = positions[vertexID];
  XMBBackgroundVertexOut output;
  output.position = float4(position, 0.0, 1.0);
  output.uvYDown = float2(
    position.x * 0.5 + 0.5,
    1.0 - (position.y * 0.5 + 0.5)
  );
  return output;
}

fragment float4 xmbBackgroundFragment(
  XMBBackgroundVertexOut input [[stage_in]],
  constant XMBBackgroundUniforms &uniforms [[buffer(0)]]
) {
  float2 direction = uniforms.directionRange.xy;
  float minimum = uniforms.directionRange.z;
  float span = max(uniforms.directionRange.w, 1e-6);
  float2 offsetUV = input.uvYDown - uniforms.geometry.xy;
  float projection = dot(offsetUV, direction);
  float2 perpendicular = float2(-direction.y, direction.x);
  float perpendicularPosition = dot(
    offsetUV - float2(0.5),
    perpendicular
  );
  projection += uniforms.geometry.w
    * perpendicularPosition
    * perpendicularPosition
    * span
    * 0.78;
  float normalizedProgress = (projection - minimum) / span;
  float progress = clamp(
    (normalizedProgress - 0.5) / max(uniforms.geometry.z, 0.05) + 0.5,
    0.0,
    1.0
  );
  float smoothProgress = progress * progress * (3.0 - 2.0 * progress);
  return float4(
    mix(uniforms.startColor.rgb, uniforms.endColor.rgb, smoothProgress),
    1.0
  );
}

struct XMBWaveUniforms {
  float4 timing;
  float4 geometry;
  float4 shaping;
  float4 detail;
  float4 ffdScale1;
  float4 ffdScale2;
  float4 ffdOffset;
  float4 material;
  float4 color;
};

struct XMBWaveVertexOut {
  float4 position [[position]];
  float3 worldPosition;
};

vertex XMBWaveVertexOut xmbWaveVertex(
  uint vertexID [[vertex_id]],
  const device float2 *positions [[buffer(0)]],
  constant XMBWaveUniforms &uniforms [[buffer(1)]],
  texture2d<float> splineTexture [[texture(0)]]
) {
  constexpr sampler splineSampler(
    coord::normalized,
    address::clamp_to_edge,
    filter::linear
  );
  float2 inputPosition = positions[vertexID];
  float3 position = float3(inputPosition.x, 0.0, inputPosition.y);
  float2 uv = (inputPosition + 1.0) * 0.5;
  position.y = splineTexture.sample(splineSampler, uv).r;

  float time = uniforms.timing.x;
  float flowSpeed = uniforms.timing.y;
  float tension = uniforms.timing.z;
  float damping = uniforms.timing.w;
  float length = uniforms.geometry.x;
  float spacing = uniforms.geometry.y;
  float perturbation = uniforms.geometry.z;
  float perturbationScale = uniforms.geometry.w;
  float timeStep = uniforms.shaping.x;
  float waveCosineAmplitude = uniforms.shaping.y;
  float waveBias = uniforms.shaping.z;
  float waveHeightScale = uniforms.shaping.w;

  float3 ffd1 = position * uniforms.ffdScale1.xyz + uniforms.ffdOffset.xyz;
  float3 ffd2 = position * uniforms.ffdScale2.xyz + uniforms.ffdOffset.xyz;
  position.y += sin(ffd1.x + time * flowSpeed) * uniforms.detail.y;
  position.z += cos(ffd2.z + time * flowSpeed) * uniforms.detail.z;

  float baseWave = cos(position.x * 2.0 - time * 0.5 * timeStep)
    * waveCosineAmplitude + waveBias;
  baseWave *= 1.0 - damping;
  baseWave += tension
    * sin(position.x * length + time * flowSpeed * timeStep * 0.25);
  float structured = perturbation * perturbationScale * (
    sin(
      (position.x * length * 6.0 + position.z * 0.5)
        * spacing * 0.01
        + time * flowSpeed * timeStep * 0.7
    ) * 0.5
      + sin(
        (position.x * length * 10.0 - position.z * 0.8)
          * spacing * 0.005
          - time * flowSpeed * timeStep * 0.35
      ) * 0.25
  );
  float totalWave = (baseWave + structured) * waveHeightScale;
  float softClip = max(uniforms.detail.x, 1e-4);
  totalWave = softClip * tanh(totalWave / softClip);
  position.y -= totalWave;

  float2 shiftedUV = uv;
  shiftedUV.x = fract(
    shiftedUV.x - time * flowSpeed * 0.04 * timeStep
  );
  position.z -= splineTexture.sample(splineSampler, shiftedUV).r
    * uniforms.detail.w;

  XMBWaveVertexOut output;
  output.position = float4(position, 1.0);
  output.worldPosition = position;
  return output;
}

fragment float4 xmbWaveFragment(
  XMBWaveVertexOut input [[stage_in]],
  constant XMBWaveUniforms &uniforms [[buffer(1)]]
) {
  float3 derivativeX = dfdx(input.worldPosition);
  float3 derivativeY = dfdy(input.worldPosition);
  float3 normal = normalize(cross(derivativeX, derivativeY));
  float fresnel = uniforms.material.w * pow(
    1.0 + dot(float3(0.0, 0.0, -1.0), normal),
    uniforms.material.z
  );
  float alpha = fresnel * uniforms.material.x * uniforms.material.y;
  return float4(uniforms.color.rgb, alpha);
}

struct XMBParticleUniforms {
  float4 timing;
  float4 sizing;
  float4 color;
  float4 distribution;
  float4 waveFollowing;
};

struct XMBParticleVertexOut {
  float4 position [[position]];
  float pointSize [[point_size]];
  float alpha;
};

vertex XMBParticleVertexOut xmbParticleVertex(
  uint vertexID [[vertex_id]],
  const device float3 *seeds [[buffer(0)]],
  constant XMBParticleUniforms &uniforms [[buffer(1)]],
  constant XMBWaveUniforms &waveUniforms [[buffer(2)]],
  texture2d<float> splineTexture [[texture(0)]]
) {
  constexpr sampler splineSampler(
    coord::normalized,
    address::clamp_to_edge,
    filter::linear
  );
  float3 seed = seeds[vertexID];
  float time = uniforms.timing.x * uniforms.timing.y;
  float ratio = uniforms.timing.z;
  float horizontalVelocity = uniforms.distribution.z < 0.5
    ? seed.x - 0.5
    : abs(seed.x - 0.5) * uniforms.distribution.x;
  float x = fract(time * horizontalVelocity / 15.0 + seed.y * 50.0)
    * 2.0 - 1.0;
  float flowOffset = sin(
    sign(seed.y) * time * (seed.y + 1.5) / 4.0 + seed.x * 100.0
  ) / ((6.0 - seed.x * 4.0 * seed.y) / ratio);
  float opacityVariation = mix(
    sin(time * (seed.x + 0.5) * 12.0 + seed.y * 10.0),
    sin(time * (seed.y + 1.5) * 6.0 + seed.x * 4.0),
    flowOffset * 0.5 + 0.5
  ) * seed.x + seed.y;

  float outerSeed = seed.y * 2.0 - 1.0;
  float outerOffset = sign(outerSeed)
    * pow(abs(outerSeed), 0.45)
    * uniforms.distribution.w
    * 0.16;
  float waveY = 0.0;
  if (uniforms.waveFollowing.x > 0.5) {
    float3 wavePosition = float3(x, 0.0, 0.0);
    float2 waveUV = (wavePosition.xz + 1.0) * 0.5;
    wavePosition.y = splineTexture.sample(splineSampler, waveUV).r;

    float3 ffd1 = wavePosition * waveUniforms.ffdScale1.xyz
      + waveUniforms.ffdOffset.xyz;
    float3 ffd2 = wavePosition * waveUniforms.ffdScale2.xyz
      + waveUniforms.ffdOffset.xyz;
    wavePosition.y += sin(
      ffd1.x + waveUniforms.timing.x * waveUniforms.timing.y
    ) * waveUniforms.detail.y;
    wavePosition.z += cos(
      ffd2.z + waveUniforms.timing.x * waveUniforms.timing.y
    ) * waveUniforms.detail.z;

    float baseWave = cos(
      wavePosition.x * 2.0
        - waveUniforms.timing.x * 0.5 * waveUniforms.shaping.x
    ) * waveUniforms.shaping.y + waveUniforms.shaping.z;
    baseWave *= 1.0 - waveUniforms.timing.w;
    baseWave += waveUniforms.timing.z * sin(
      wavePosition.x * waveUniforms.geometry.x
        + waveUniforms.timing.x
          * waveUniforms.timing.y
          * waveUniforms.shaping.x
          * 0.25
    );
    float structured = waveUniforms.geometry.z * waveUniforms.geometry.w * (
      sin(
        (wavePosition.x * waveUniforms.geometry.x * 6.0
          + wavePosition.z * 0.5)
          * waveUniforms.geometry.y
          * 0.01
          + waveUniforms.timing.x
            * waveUniforms.timing.y
            * waveUniforms.shaping.x
            * 0.7
      ) * 0.5
        + sin(
          (wavePosition.x * waveUniforms.geometry.x * 10.0
            - wavePosition.z * 0.8)
            * waveUniforms.geometry.y
            * 0.005
            - waveUniforms.timing.x
              * waveUniforms.timing.y
              * waveUniforms.shaping.x
              * 0.35
        ) * 0.25
    );
    float totalWave = (baseWave + structured) * waveUniforms.shaping.w;
    float softClip = max(waveUniforms.detail.x, 1e-4);
    totalWave = softClip * tanh(totalWave / softClip);
    waveY = wavePosition.y - totalWave;
  }

  float y = waveY
    + uniforms.sizing.w
    + flowOffset * uniforms.distribution.y
    + outerOffset;

  XMBParticleVertexOut output;
  output.position = float4(x, y, 0.0, 1.0);
  float depthScale = max(
    0.25,
    1.0 + (seed.x - 0.5) * uniforms.color.a
  );
  output.pointSize = (seed.z * uniforms.sizing.y + uniforms.sizing.x)
    * uniforms.sizing.z
    * depthScale;
  output.alpha = opacityVariation * opacityVariation
    * (1.0 - fract(seed.x + time * 0.00285));
  return output;
}

fragment float4 xmbParticleFragment(
  XMBParticleVertexOut input [[stage_in]],
  float2 pointCoordinate [[point_coord]],
  constant XMBParticleUniforms &uniforms [[buffer(1)]]
) {
  float2 centered = pointCoordinate * 2.0 - 1.0;
  float distanceSquared = dot(centered, centered);
  if (distanceSquared > 1.0) {
    discard_fragment();
  }
  float sparkle = (1.0 - distanceSquared) * (1.0 - distanceSquared);
  float alpha = input.alpha * uniforms.timing.w * sparkle;
  bool rendersTransparentParticles = uniforms.distribution.z >= 0.5
    || uniforms.waveFollowing.y > 0.5;
  float outputAlpha = rendersTransparentParticles ? alpha : 1.0;
  return float4(uniforms.color.rgb * alpha, outputAlpha);
}
"""#
}

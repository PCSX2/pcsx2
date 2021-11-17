import { Octokit } from "@octokit/rest";
import { throttling } from "@octokit/plugin-throttling";
import { retry } from "@octokit/plugin-retry";
Octokit.plugin(throttling);
Octokit.plugin(retry);
const octokit = new Octokit({
  auth: process.env.GITHUB_TOKEN,
  userAgent: 'PCSX2/pcsx2',
  log: {
    debug: () => { },
    info: () => { },
    warn: console.warn,
    error: console.error
  },
  throttle: {
    onRateLimit: (retryAfter, options) => {
      octokit.log.warn(
        `Request quota exhausted for request ${options.method} ${options.url}`
      );

      // Retry twice after hitting a rate limit error, then give up
      if (options.request.retryCount <= 2) {
        console.log(`Retrying after ${retryAfter} seconds!`);
        return true;
      }
    },
    onAbuseLimit: (retryAfter, options) => {
      // does not retry, only logs a warning
      octokit.log.warn(
        `Abuse detected for request ${options.method} ${options.url}`
      );
    },
  }
});

let assetDir = process.env.ASSET_DIR;
let tagToSearchFor = process.env.TAG_TO_SEARCH_FOR.split("refs/tags/")[1];

console.log(`Searching in - ${assetDir}`);
console.log(`Searching for tag - ${tagToSearchFor}`);

const { data: recentReleases } = await octokit.rest.repos.listReleases({
  owner: "PCSX2",
  repo: "pcsx2",
  per_page: 100
});

let release = undefined;
for (var i = 0; i < recentReleases.length; i++) {
  if (recentReleases[i].tag_name == tagToSearchFor) {
    release = recentReleases[i];
    break;
  }
}

if (release == undefined) {
  console.log(`Unable to find release with tag - ${tagToSearchFor}`);
  process.exit(1);
}

// Upload any assets we need to, don't upload assets that are already there!
const { data: releaseAssetsPre } = await octokit.rest.repos.listReleaseAssets({
  owner: "PCSX2",
  repo: "pcsx2",
  release_id: release.id,
  per_page: 100
});

import glob from 'glob';
import * as fs from 'fs';
import * as path from 'path';
glob(assetDir + `/**/*${process.env.ASSET_EXTENSION}`, {}, async (err, files) => {
  for (var i = 0; i < files.length; i++) {
    let foundDuplicate = false;
    for (var j = 0; j < releaseAssetsPre.length; j++) {
      let existingAsset = releaseAssetsPre[j];
      if (existingAsset.name == `pcsx2-${release.tag_name}-${path.basename(files[i])}`) {
        foundDuplicate = true;
        break;
      }
    }
    if (foundDuplicate) {
      continue;
    }

    var assetBytes = fs.readFileSync(files[i], null);
    const { data: uploadAsset } = await octokit.rest.repos.uploadReleaseAsset({
      owner: "PCSX2",
      repo: "pcsx2",
      release_id: release.id,
      name: `pcsx2-${release.tag_name}-${path.basename(files[i])}`,
      data: assetBytes,
    });
  }
});

// Ideally there would be a webhook event for when an artifact is added to a draft release
// unfortunately, such a thing does not exist yet.  Therefore, we have to wait a bit
// for the API to become consistent
// TODO - future work - we could check previous draft releases to become eventually consistent as well
// - draft releases should only be a temporary state, so anything that remains a draft had a problem
await new Promise(resolve => setTimeout(resolve, 10 * 1000));

// Poll the release, and check to see if it's ready to be published
const { data: releaseAssetsPost } = await octokit.rest.repos.listReleaseAssets({
  owner: "PCSX2",
  repo: "pcsx2",
  release_id: release.id,
  per_page: 100
});

// Expected Assets, if we have all of them, we will publish it
let expectedAssets = {
  "windows-32bit-sse4": false,
  "windows-32bit-avx2": false,
  "windows-64bit-sse4": false,
  "windows-64bit-avx2": false,
  "linux-appimage-32bit": false,
  "linux-appimage-64bit": false
}

for (var i = 0; i < releaseAssetsPost.length; i++) {
  let asset = releaseAssetsPost[i];
  if (asset.name.includes("symbols")) {
    continue;
  }
  for (var j = 0; j < Object.keys(expectedAssets).length; j++) {
    let expectedNamePrefix = Object.keys(expectedAssets)[j];
    if (asset.name.toLowerCase().includes(expectedNamePrefix)) {
      expectedAssets[expectedNamePrefix] = true;
      break;
    }
  }
}

console.log(expectedAssets);

if (Object.values(expectedAssets).every(Boolean)) {
  await octokit.rest.repos.updateRelease({
    owner: "PCSX2",
    repo: "pcsx2",
    release_id: release.id,
    draft: false
  });
}

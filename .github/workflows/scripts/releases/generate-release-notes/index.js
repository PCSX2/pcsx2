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

var args = process.argv.slice(2);
let commitSha = process.env.COMMIT_SHA;

console.log(`Searching for Commit - ${commitSha}`);

const { data: commit } = await octokit.rest.repos.getCommit({
  owner: "PCSX2",
  repo: "pcsx2",
  ref: commitSha,
});

const { data: associatedPulls } = await octokit.rest.repos.listPullRequestsAssociatedWithCommit({
  owner: "PCSX2",
  repo: "pcsx2",
  commit_sha: commit.sha,
});

let releaseNotes = ``;

if (associatedPulls.length === 0) {
  releaseNotes += `- ${commit.commit.message}\n`;
} else {
  for (var j = 0; j < associatedPulls.length; j++) {
    releaseNotes += `- [${associatedPulls[j].title}](${associatedPulls[j].html_url})\n`;
  }
}

import * as fs from 'fs';
fs.writeFileSync('./release-notes.md', releaseNotes);

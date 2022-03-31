import { MessageEmbed, WebhookClient } from "discord.js";
import * as github from '@actions/github';

const releaseInfo = github.context.payload.release;

if (!releaseInfo.prerelease) {
  console.log("Not announcing - release was not a pre-release (aka a Nightly)");
  process.exit(0);
}

// Publish Webhook
const embed = new MessageEmbed()
  .setColor('#FF8000')
  .setTitle('New PCSX2 Nightly Build Available!')
  .setDescription("To download the latest or previous builds, [visit the official downloads page](https://pcsx2.net/downloads/).")
  .addFields(
    { name: 'Version', value: releaseInfo.tag_name, inline: true },
    { name: 'Installation Steps', value: '[See Here](https://github.com/PCSX2/pcsx2/wiki/Nightly-Build-Usage-Guide)', inline: true },
    { name: 'Included Changes', value: releaseInfo.body, inline: false }
  );

const webhookClient = new WebhookClient({ url: process.env.DISCORD_BUILD_WEBHOOK });
await webhookClient.send({
  embeds: [embed],
});

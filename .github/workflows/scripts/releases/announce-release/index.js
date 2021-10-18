import { MessageEmbed, WebhookClient } from "discord.js";
import * as github from '@actions/github';

const assets = github.context.payload.release.assets;
let windowsAssetLinks = "";
let linuxAssetLinks = "";

for (var i = 0; i < assets.length; i++) {
  let asset = assets[i];
  if (asset.name.includes("symbols")) {
    continue;
  }
  if (asset.name.includes("windows")) {
    windowsAssetLinks += `- [${asset.name}](${asset.browser_download_url})\n`
  } else if (asset.name.includes("linux")) {
    linuxAssetLinks += `- [${asset.name}](${asset.browser_download_url})\n`
  }
}

// Publish Webhook
const embed = new MessageEmbed()
  .setColor('#FF8000')
  .setTitle('New PCSX2 Nightly Build Available!')
  .addFields(
    { name: 'Version', value: github.context.payload.release.tag_name, inline: true },
    { name: 'Release Link', value: `[Github Release](${github.context.payload.release.html_url})`, inline: true },
    { name: 'Installation Steps', value: '[See Here](https://github.com/PCSX2/pcsx2/wiki/Nightly-Build-Usage-Guide)', inline: true }
  );

if (windowsAssetLinks != "") {
  embed.addField('Windows Downloads', windowsAssetLinks, false);
}
if (linuxAssetLinks != "") {
  embed.addField('Linux Downloads', linuxAssetLinks, false);
}

const webhookClient = new WebhookClient({ url: process.env.DISCORD_BUILD_WEBHOOK });
await webhookClient.send({
  embeds: [embed],
});

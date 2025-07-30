import dotenv from 'dotenv';
import bind from 'bindings';
import { Client, GatewayIntentBits, REST, Routes, SlashCommandBuilder, EmbedBuilder, MessageFlags } from 'discord.js';
import { unzip, HTTPRangeReader } from 'unzipit';

dotenv.config();

const FAILED_LOGS = ['exception', 'badgpu', 'graphics'];
const MAX_LOG_SIZE = 10 * 1024 * 1024;
const MAX_EMBEDS = 3;

const logan = bind('./Release/psOff_logan');

const client = new Client({
	intents: [
		GatewayIntentBits.Guilds,
		GatewayIntentBits.GuildMessages,
		GatewayIntentBits.MessageContent,
	],
});

client.once('ready', () => {
	console.log(`Logged in as ${client.user.tag}!`);
});

const commands = [
	new SlashCommandBuilder()
		.setName('logan')
		.setDescription('Analyze the attached log file')
		.addAttachmentOption((option) =>
			option
				.setName('logfile')
				.setDescription('The log file to check')
				.setRequired(true),
		),
];

const rest = new REST({ version: '10' }).setToken(process.env.DISCORD_TOKEN);

(async () => {
	try {
		console.log('Creating application guild commands.');
		await rest.put(
			Routes.applicationGuildCommands(process.env.CLIENT_ID, process.env.GUILD_ID),
			{ body: commands },
		);
		console.log('Successfully created application guild commands.');
	} catch (error) {
		console.error(error);
	}
})();

const createEmbedWithError = (title, descr) => {
	return new EmbedBuilder()
		.setColor('#A00011')
		.setTitle(title)
		.setDescription(descr);
};

const createEmbedFromLog = (interaction, logData) => {
	const embed = new EmbedBuilder();

	const hintsAndLables =
		(logData.labels.length > 0 ? `\n**Possible GitHub issue labels**:\n${logData.labels.map(item => `* ${item}`).join('\n')}\n` : '') +
		(logData.hints.length > 0 ? `\n**Hints**:\n${logData.hints.map(item => `* ${item}`).join('\n')}\n` : '');

	const builders = {
		'main-process': () =>
			`**User's GPU**: ${logData['user-gpu']}\n` +
			`**User's language**: ${logData['user-lang']}\n` + hintsAndLables,

		'child-process': () =>
			`**Title ID**: ${logData['title_id']}\n` +
			`**PS4 Pro mode**: ${logData['title_neo'] ? 'Yes' : 'No'}\n` + hintsAndLables +
			(logData.firmware.length > 0 ? `\n**Loaded PS4 firmware libraries**:\n${logData.firmware.map(item => `* ${item}`).join('\n')}\n` : ''),
	};

	embed
		.setColor(FAILED_LOGS.some(item => logData.labels.includes(item)) ? '#A00011' : '#00A011')
		.setTitle(logData.__filename)
		.setDescription(`Here\'s your log file information:\n\n**Log Type**: ${logData.type}\n` + builders[logData.type]());

	return embed;
};

client.on('interactionCreate', async (interaction) => {
	if (!interaction.isCommand()) return;
	if (interaction.channelId !== process.env.CHANNEL_ID) {
		return interaction.reply({
			content: `You can\'t use this command here, you have to go to <#${process.env.CHANNEL_ID}>`,
			flags: MessageFlags.Ephemeral,
		});
	}

	const { commandName } = interaction;

	if (commandName === 'logan') {
		await interaction.deferReply();

		const attachment = interaction.options.getAttachment('logfile');

		if (!attachment) {
			await interaction.editReply('Please attach a log file to the command.');
			return;
		}

		const isZip = attachment.name.endsWith('.zip');
		if (!attachment.name.endsWith('.p7d') && !isZip) {
			await interaction.editReply('Please attach a .p7d or .zip file.');
			return;
		}

		try {
			if (isZip) {
				const reader = new HTTPRangeReader(attachment.url)
				const zip = await unzip(reader);

				const embeds = [];
				let text = '';

				for (const [filename, file] of Object.entries(zip.entries)) {
					try {
						if (embeds.length >= MAX_EMBEDS) {
							text = `Too many log files inside zip archive, we\'re processed only first ${embeds.length} of them.`;
							break;
						}
						if (file.dir) continue;
						if (!filename.endsWith('.p7d')) continue;
						if (file.encrypted) {
							embeds.push(createEmbedWithError(filename, 'This file is encrypted, we can\'t process it'));
							continue;
						}
						if (file.size > MAX_LOG_SIZE) {
							embeds.push(createEmbedWithError(filename, 'This file is too big, we can\'t process it'));
							continue;
						}
						const logdata = logan.meman(await file.arrayBuffer());
						logdata.__filename = filename;
						embeds.push(createEmbedFromLog(interaction, logdata));
					} catch (error) { }
				}

				if (embeds.length === 0) {
					await interaction.editReply('No log files found in the provided zip archive.');
					return;
				}

				await interaction.editReply({ content: text, embeds });
			} else {
				await interaction.editReply('Downloading your log file...');
				await fetch(attachment.url).then(async (response) => {
					await interaction.editReply('Log file downloaded! Analyzing...');
					const abuffer = await response.arrayBuffer();
					const logdata = logan.meman(abuffer);
					logdata.__filename = attachment.name;
					await interaction.editReply({ content: '', embeds: [createEmbedFromLog(interaction, logdata)] });

				});
			}
		} catch (error) {
			console.error(error);
			await interaction.editReply('Error processing the log file! Your log file might be empty or corrupted.');
		}
	}
});

client.login(process.env.DISCORD_TOKEN);

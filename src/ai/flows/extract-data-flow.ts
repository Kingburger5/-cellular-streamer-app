 'use server';

/**
 * @fileOverview An AI agent for extracting structured data from text, following specific business logic.
 *
 * - extractData - A function that extracts structured data from a text blob.
 * - ExtractDataInput - The input type for the extractData function.
 * - ExtractDataOutput - The return type for the extractData function.
 */

import {ai} from '@/ai/genkit';
import {z} from 'zod';
import { add, format } from 'date-fns';

const DataPointSchema = z.object({
  siteName: z.string().optional().describe('The name of the site, extracted from the filename (part before the first underscore).'),
  surveyDate: z.string().optional().describe('The date of the survey in YYYY-MM-DD format, from the timestamp.'),
  surveyFinishTime: z.string().optional().describe('The finish time of the survey in HH:mm:ss format, calculated by adding the recording length to the timestamp.'),
  timestamp: z.string().describe('The timestamp of the data point (e.g., "2025-03-02 20:50:16+13:00").'),
  latitude: z.number().describe('The latitude of the location.'),
  longitude: z.number().describe('The longitude of the location.'),
  temperature: z.number().describe('The temperature reading.'),
  length: z.number().optional().describe('The length of the recording in seconds.'),
  flybys: z.number().optional().describe('The number of fly-bys. This field may not be present.'),
  sampleRate: z.number().optional().describe('The sample rate in Hz (e.g., 256000).'),
  minTriggerFreq: z.number().optional().describe('The minimum trigger frequency in Hz.'),
  maxTriggerFreq: z.number().optional().describe('The maximum trigger frequency in Hz.'),
  make: z.string().optional().describe('The make of the recording device (e.g., "Wildlife Acoustics, Inc.").'),
  model: z.string().optional().describe('The model of the recording device (e.g., "Song Meter Mini Bat").'),
  serial: z.string().optional().describe('The serial number of the recording device (e.g., "SMU06612").'),
  firmwareVersion: z.string().optional().describe("The firmware version of the device."),
  gain: z.number().optional().describe("The gain setting from the audio settings."),
  triggerWindow: z.number().optional().describe("The trigger window in seconds."),
  triggerMaxLen: z.number().optional().describe("The maximum trigger length in seconds."),
  triggerMinDur: z.number().optional().describe("The minimum trigger duration in seconds."),
  triggerMaxDur: z.number().optional().describe("The maximum trigger duration in seconds."),
});

const ExtractDataInputSchema = z.object({
  fileContent: z.string().describe('The text content to extract data from.'),
  filename: z.string().describe('The original filename of the uploaded file.'),
});
export type ExtractDataInput = z.infer<typeof ExtractDataInputSchema>;

const ExtractDataOutputSchema = z.object({
  data: z.array(DataPointSchema).describe('The extracted structured data points.'),
});
export type ExtractDataOutput = z.infer<typeof ExtractDataOutputSchema>;

export async function extractData(input: ExtractDataInput): Promise<ExtractDataOutput> {
  const aiResult = await extractDataFlow(input);

  // Post-process to calculate derived fields
  if (aiResult.data) {
    aiResult.data = aiResult.data.map(point => {
        let surveyDate: string | undefined = undefined;
        let surveyFinishTime: string | undefined = undefined;
        
        try {
          const startDate = new Date(point.timestamp);
          surveyDate = format(startDate, 'yyyy-MM-dd');
          
          if(point.length) {
            const finishDate = add(startDate, { seconds: point.length });
            surveyFinishTime = format(finishDate, 'HH:mm:ss');
          }
        } catch (e) {
            console.error("Could not parse date for calculation", e);
        }

        return {
            ...point,
            surveyDate,
            surveyFinishTime
        };
    });
  }

  return aiResult;
}

const prompt = ai.definePrompt({
  name: 'extractDataPrompt',
  input: {schema: ExtractDataInputSchema},
  output: {schema: ExtractDataOutputSchema},
  prompt: `You are an expert at extracting structured data from text based on specific rules.
The provided text is metadata from a file, in GUANO format. The format consists of key-value pairs, which are on new lines. Some lines may contain a JSON string.

Your task is to parse this text and extract the relevant data points into a single data point object.

**Extraction Rules:**
- **siteName**: Extract the part of the filename *before* the first underscore ('_'). For example, from 'SITE1_2025..._...wav', extract 'SITE1'.
- The 'Loc Position' field contains both latitude and longitude, separated by a space. You must extract them into the separate 'latitude' and 'longitude' fields.
- The temperature might be labeled as 'Temperature Int'. Extract its numeric value.
- The 'Samplerate' field should be extracted as 'sampleRate'.
- From a key-value pair like 'Make:Wildlife Acoustics, Inc.', extract 'Wildlife Acoustics, Inc.' for the 'make' field.
- From a key-value pair like 'Model:Song Meter Mini Bat', extract 'Song Meter Mini Bat' for the 'model' field.
- From a key-value pair like 'Serial:SMU06612', extract 'SMU06612' for the 'serial' field.
- Extract 'Firmware Version' as 'firmwareVersion'.
- **CRITICAL**: The 'Audio settings' field contains a JSON string inside an array. You MUST parse this JSON to extract the following values:
  - The value of "gain" from the JSON should be extracted to the 'gain' field.
  - The value of "trig window" from the JSON should be extracted to the 'triggerWindow' field.
  - The value of "trig max len" from the JSON should be extracted to the 'triggerMaxLen' field.
  - The value of "trig min freq" from the JSON should be extracted to the 'minTriggerFreq' field.
  - The value of "trig max freq" from the JSON should be extracted to the 'maxTriggerFreq' field.
  - The value of "trig min dur" from the JSON should be extracted to the 'triggerMinDur' field.
  - The value of "trig max dur" from the JSON should be extracted to the 'triggerMaxDur' field.
- If a field is not present in the text, you should omit it from the output for that data point.
- If no parsable data is found, return an empty array for the 'data' field.

**Example Input Text:**
GUANO|Version:1.0|Firmware Version:4.6|Make:Wildlife Acoustics, Inc.|Model:Song Meter Mini Bat|Serial:SMU06612|WA|Song Meter|Prefix:1|WA|Song Meter|Audio settings:[{"rate":256000,"gain":12,"trig window":3.0,"trig max len":15.0,"trig min freq":30000,"trig max freq":128000,"trig min dur":0.0015,"trig max dur":0.0000}]|Length:3.921|Original Filename:1_20250302_205009.wav|Timestamp:2025-03-02 20:50:09+13:00|Loc Position:-37.00403 174.57577|Temperature Int:20.75|Samplerate:256000

**Example Filename:**
"HADES_20250302_205016.wav"

**From this example, you must extract a single JSON object with these fields:**
- siteName: "HADES"
- timestamp: "2025-03-02 20:50:09+13:00"
- latitude: -37.00403
- longitude: 174.57577
- temperature: 20.75
- length: 3.921
- sampleRate: 256000
- make: "Wildlife Acoustics, Inc."
- model: "Song Meter Mini Bat"
- serial: "SMU06612"
- firmwareVersion: "4.6"
- gain: 12
- triggerWindow: 3.0
- triggerMaxLen: 15.0
- minTriggerFreq: 30000
- maxTriggerFreq: 128000
- triggerMinDur: 0.0015
- triggerMaxDur: 0.0000
- **DO NOT** calculate surveyDate or surveyFinishTime yourself. The system will do this.

Now, please process the following content:

Filename: {{{filename}}}
Content:
{{{fileContent}}}
`,
});

const extractDataFlow = ai.defineFlow(
  {
    name: 'extractDataFlow',
    inputSchema: ExtractDataInputSchema,
    outputSchema: ExtractDataOutputSchema,
  },
  async (input) => {
    const {output} = await prompt(input);
    return output!;
  }
);

    
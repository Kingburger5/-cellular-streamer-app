 'use server';

/**
 * @fileOverview An AI agent for extracting structured data from text.
 *
 * - extractData - A function that extracts structured data from a text blob.
 * - ExtractDataInput - The input type for the extractData function.
 * - ExtractDataOutput - The return type for the extractData function.
 */

import {ai} from '@/ai/genkit';
import {z} from 'genkit';

const DataPointSchema = z.object({
  timestamp: z.string().describe('The timestamp of the data point (e.g., "2025-03-02 20:50:16+13:00").'),
  latitude: z.number().describe('The latitude of the location.'),
  longitude: z.number().describe('The longitude of the location.'),
  temperature: z.number().describe('The temperature reading.'),
  flybys: z.number().optional().describe('The number of fly-bys. This field may not be present.'),
  sampleRate: z.number().optional().describe('The sample rate in Hz.'),
  minTriggerFreq: z.number().optional().describe('The minimum trigger frequency in Hz.'),
  maxTriggerFreq: z.number().optional().describe('The maximum trigger frequency in Hz.'),
  make: z.string().optional().describe('The make of the recording device (e.g., "Wildlife Acoustics, Inc.").'),
  model: z.string().optional().describe('The model of the recording device (e.g., "Song Meter Mini Bat").'),
});

const ExtractDataInputSchema = z.object({
  fileContent: z.string().describe('The text content to extract data from.'),
});
export type ExtractDataInput = z.infer<typeof ExtractDataInputSchema>;

const ExtractDataOutputSchema = z.object({
  data: z.array(DataPointSchema).describe('The extracted structured data points.'),
});
export type ExtractDataOutput = z.infer<typeof ExtractDataOutputSchema>;

export async function extractData(input: ExtractDataInput): Promise<ExtractDataOutput> {
  return extractDataFlow(input);
}

const prompt = ai.definePrompt({
  name: 'extractDataPrompt',
  input: {schema: ExtractDataInputSchema},
  output: {schema: ExtractDataOutputSchema},
  prompt: `You are an expert at extracting structured data from text. The provided text is metadata from a file, in GUANO format.

Your task is to parse this text and extract the relevant data points into a single data point object.

- The 'Loc Position' field contains both latitude and longitude, separated by a space. You must extract them into the separate 'latitude' and 'longitude' fields.
- The temperature might be labeled as 'Temperature Int'.
- The 'Samplerate' field should be extracted as 'sampleRate'.
- The 'Make' and 'Model' fields should be extracted into 'make' and 'model'.
- The 'Audio settings' field is a JSON string. From this, you must extract 'trig min freq' as 'minTriggerFreq' and 'trig max freq' as 'maxTriggerFreq'.
- If a field like 'flybys' is not present in the text, you should omit it from the output for that data point.
- If no parsable data is found, return an empty array for the 'data' field.

Here is an example of the input format:
"GUANO|Version:1.0|Firmware Version:4.6|Make:Wildlife Acoustics, Inc.|Model:Song Meter Mini Bat|Serial:SMU06612|WA|Song Meter|Prefix:1|WA|Song Meter|Audio settings:[{"rate":256000,"gain":12,"trig window":3.0,"trig max len":15.0,"trig min freq":30000,"trig max freq":128000,"trig min dur":0.0015,"trig max dur":0.0000}]|Length:4.208|Original Filename:1_20250302_205016.wav|Timestamp:2025-03-02 20:50:16+13:00|Loc Position:-37.00403 174.57577|Temperature Int:20.75|Samplerate:256000"

From this example, you must extract:
- timestamp: "2025-03-02 20:50:16+13:00"
- latitude: -37.00403
- longitude: 174.57577
- temperature: 20.75
- sampleRate: 256000
- minTriggerFreq: 30000
- maxTriggerFreq: 128000
- make: "Wildlife Acoustics, Inc."
- model: "Song Meter Mini Bat"

Now, please process the following content:

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

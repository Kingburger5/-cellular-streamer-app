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
  prompt: `You are an expert at extracting structured data from GUANO metadata text. The provided text consists of key-value pairs, separated by pipes ('|') which should be treated as newlines.

Your task is to parse this text and extract the relevant data points into a single, structured data point object.

**Extraction Rules (CRITICAL):**

1.  **siteName**: Extract the part of the filename *before* the first underscore ('_'). For example, from 'HADES_2025..._...wav', extract 'HADES'.
2.  **Loc Position**: This field contains Latitude and Longitude as two numbers separated by a space (e.g., "-37.00403 174.57577"). You MUST extract the first number into the 'latitude' field and the second number into the 'longitude' field.
3.  **Temperature Int**: Extract its numeric value into the 'temperature' field.
4.  **Samplerate**: Extract its numeric value into the 'sampleRate' field.
5.  **Make**: From a key like 'Make:Wildlife Acoustics, Inc.', extract 'Wildlife Acoustics, Inc.' for the 'make' field.
6.  **Model**: From a key like 'Model:Song Meter Mini Bat', extract 'Song Meter Mini Bat' for the 'model' field.
7.  **Serial**: From 'Serial:SMU06612', extract 'SMU06612' for 'serial'.
8.  **Firmware Version**: Extract the value for 'firmwareVersion'.
9.  **Audio settings**: This field contains a JSON string inside a single-element array (e.g., '[{...}]'). You MUST parse this JSON to extract the following values into their corresponding fields:
    - "gain" -> \`gain\` (number)
    - "trig window" -> \`triggerWindow\` (number)
    - "trig max len" -> \`triggerMaxLen\` (number)
    - "trig min freq" -> \`minTriggerFreq\` (number)
    - "trig max freq" -> \`maxTriggerFreq\` (number)
    - "trig min dur" -> \`triggerMinDur\` (number)
    - "trig max dur" -> \`triggerMaxDur\` (number)
10. **Other fields**: Directly map the values from the text to the corresponding field in the output schema (e.g., 'Timestamp' -> \`timestamp\`, 'Length' -> \`length\`).
11. **Omissions**: If a field is not present in the text, you must omit it from the output. Do not guess or fill with default values.
12. **System Calculations**: **DO NOT** calculate \`surveyDate\` or \`surveyFinishTime\` yourself. The system will handle this.
13. **Output**: If no parsable data is found, return an empty array for the 'data' field.

**Example Input Text:**
GUANO|Version:1.0|Firmware Version:4.6|Make:Wildlife Acoustics, Inc.|Model:Song Meter Mini Bat|Serial:SMU06612|WA|Song Meter|Prefix:1|WA|Song Meter|Audio settings:[{"rate":256000,"gain":12,"trig window":3.0,"trig max len":15.0,"trig min freq":30000,"trig max freq":128000,"trig min dur":0.0015,"trig max dur":0.0000}]|Length:3.921|Original Filename:1_20250302_205009.wav|Timestamp:2025-03-02 20:50:09+13:00|Loc Position:-37.00403 174.57577|Temperature Int:20.75|Samplerate:256000

**Example Filename:**
"HADES_20250302_205016.wav"

**Your JSON output for this example MUST BE:**
\`\`\`json
{
  "data": [
    {
      "siteName": "HADES",
      "timestamp": "2025-03-02 20:50:09+13:00",
      "latitude": -37.00403,
      "longitude": 174.57577,
      "temperature": 20.75,
      "length": 3.921,
      "sampleRate": 256000,
      "make": "Wildlife Acoustics, Inc.",
      "model": "Song Meter Mini Bat",
      "serial": "SMU06612",
      "firmwareVersion": "4.6",
      "gain": 12,
      "triggerWindow": 3.0,
      "triggerMaxLen": 15.0,
      "minTriggerFreq": 30000,
      "maxTriggerFreq": 128000,
      "triggerMinDur": 0.0015,
      "triggerMaxDur": 0.0
    }
  ]
}
\`\`\`

Now, please process the following content following all rules precisely.

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

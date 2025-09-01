
'use server';

/**
 * @fileOverview An AI agent for extracting structured data from file metadata.
 *
 * - extractData - A function that parses file metadata (like GUANO) into a structured format.
 * - ExtractDataInput - The input type for the extractData function.
 * - ExtractDataOutput - The return type for the extractData function.
 */

import {ai} from '@/ai/genkit';
import {z} from 'genkit';
import type { DataPoint } from '@/lib/types';

// This is the Zod schema for a single data point, matching our TypeScript interface.
const DataPointSchema = z.object({
  fileInformation: z.object({
    originalFilename: z.string().optional().describe('The original filename of the audio file.'),
    recordingDateTime: z.string().optional().describe('The full ISO 8601 timestamp of when the recording started.'),
    recordingDurationSeconds: z.number().optional().describe('The duration of the recording in seconds.'),
    sampleRateHz: z.number().optional().describe('The sample rate of the audio in Hertz.'),
  }),
  recorderDetails: z.object({
    make: z.string().optional().describe('The manufacturer of the recording device.'),
    model: z.string().optional().describe('The model of the recording device.'),
    serialNumber: z.string().optional().describe('The serial number of the recording device.'),
    firmwareVersion: z.string().optional().describe('The firmware version of the recording device.'),
    gainSetting: z.number().optional().describe('The gain setting of the recorder in dB.'),
  }),
  locationEnvironmentalData: z.object({
    latitude: z.number().optional().describe('The latitude of the recording location.'),
    longitude: z.number().optional().describe('The longitude of the recording location.'),
    temperatureCelsius: z.number().optional().describe('The ambient temperature in degrees Celsius.'),
  }),
  triggerSettings: z.object({
    windowSeconds: z.number().optional().describe('The trigger window in seconds.'),
    maxLengthSeconds: z.number().optional().describe('The maximum length of the trigger in seconds.'),
    minFrequencyHz: z.number().optional().describe('The minimum frequency for the trigger in Hertz.'),
    maxFrequencyHz: z.number().optional().describe('The maximum frequency for the trigger in Hertz.'),
    minDurationSeconds: z.number().optional().describe('The minimum duration for the trigger in seconds.'),
    maxDurationSeconds: z.number().optional().describe('The maximum duration for the trigger in seconds. A value of 0 indicates it is unused.'),
  }),
});


const ExtractDataInputSchema = z.object({
  fileContent: z.string().describe('The full metadata content of the file.'),
  filename: z.string().describe('The name of the file.'),
});
export type ExtractDataInput = z.infer<typeof ExtractDataInputSchema>;


// The output is an object containing an array of data points.
const ExtractDataOutputSchema = z.object({
    data: z.array(DataPointSchema).describe("An array of structured data points extracted from the file metadata."),
});
export type ExtractDataOutput = z.infer<typeof ExtractDataOutputSchema>;


export async function extractData(input: ExtractDataInput): Promise<ExtractDataOutput> {
  const result = await extractDataFlow(input);
  // Post-process to handle potential AI inconsistencies if necessary
  if (result && result.data) {
     result.data.forEach(dp => {
        // Example: if AI returns 0 for maxDuration, set to undefined so UI shows "N/A"
        if (dp.triggerSettings.maxDurationSeconds === 0) {
            dp.triggerSettings.maxDurationSeconds = undefined;
        }
     });
  }
  return result;
}


const prompt = ai.definePrompt({
  name: 'extractDataPrompt',
  input: {schema: ExtractDataInputSchema},
  output: {schema: ExtractDataOutputSchema},
  prompt: `You are an expert data extraction agent specializing in GUANO metadata from acoustic survey files. Your task is to parse the provided metadata string and convert it into a structured JSON object.

Follow these instructions precisely:
1.  Analyze the entire GUANO string provided in the file content.
2.  Extract every available key-value pair.
3.  The 'Audio settings' field contains a JSON array within the string. You MUST parse this nested JSON to extract its values (e.g., "rate", "gain", "trig window").
4.  The 'Loc Position' field contains latitude and longitude separated by a space. You must split these into two separate number fields.
5.  Populate the output JSON object with all the data you have extracted.
6.  If a value is not present in the GUANO metadata, omit the corresponding key from the output. Do not guess or invent data.
7.  Pay close attention to data types. Ensure numbers are represented as numbers, not strings.

Here is an example of the mapping:
- 'Original Filename' -> fileInformation.originalFilename
- 'Timestamp' -> fileInformation.recordingDateTime
- 'Length' -> fileInformation.recordingDurationSeconds
- 'Samplerate' -> fileInformation.sampleRateHz
- 'Make' -> recorderDetails.make
- 'Model' -> recorderDetails.model
- 'Serial' -> recorderDetails.serialNumber
- 'Firmware Version' -> recorderDetails.firmwareVersion
- 'gain' (from Audio settings) -> recorderDetails.gainSetting
- 'Loc Position' (first number) -> locationEnvironmentalData.latitude
- 'Loc Position' (second number) -> locationEnvironmentalData.longitude
- 'Temperature Int' -> locationEnvironmentalData.temperatureCelsius
- 'trig window' (from Audio settings) -> triggerSettings.windowSeconds
- 'trig max len' (from Audio settings) -> triggerSettings.maxLengthSeconds
- 'trig min freq' (from Audio settings) -> triggerSettings.minFrequencyHz
- 'trig max freq' (from Audio settings) -> triggerSettings.maxFrequencyHz
- 'trig min dur' (from Audio settings) -> triggerSettings.minDurationSeconds
- 'trig max dur' (from Audio settings) -> triggerSettings.maxDurationSeconds

File to process: {{{filename}}}
Metadata Content:
{{{fileContent}}}

Now, provide the structured JSON output.`,
});

const extractDataFlow = ai.defineFlow(
  {
    name: 'extractDataFlow',
    inputSchema: ExtractDataInputSchema,
    outputSchema: ExtractDataOutputSchema,
  },
  async input => {
    const {output} = await prompt(input);
    return output!;
  }
);

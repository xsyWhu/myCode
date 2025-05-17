import { Logger } from './Logger';
import { FileReader } from './FileReader';

import type { BusinessError } from '@kit.BasicServicesKit';
import type { Context } from '@kit.AbilityKit';
import { insightIntent } from '@kit.IntentsKit';

interface DeleteIntent {
  intentName:string;
  identifiers:string[];
}

interface DeleteEntity {
  entityName:string;
  entityIds:string[];
}

const TAG = 'InsightIntentSharer';

export class InsightIntentSharer {
  static async refreshShareIntentMessage(context: Context): Promise<string> {
    Logger.debug(TAG, 'refreshShareIntentMessage');
    try {
      return await FileReader.readJson(context, "shareIntent.json");
    } catch (error) {
      Logger.error(TAG, `refresh share intent message failed: ${JSON.stringify(error)}`);
      return 'invalid share intent message';
    }
  }

  static async shareIntent(context: Context, input: string): Promise<string> {
    Logger.debug(TAG, 'shareIntent');
    let insightIntents: insightIntent.InsightIntent[] = JSON.parse(input);
    if (!insightIntents || insightIntents.length === 0){
      Logger.error(TAG, 'shareIntent: json invalid.');
      return 'shareIntent: json invalid.';
    }
    return await insightIntent.shareIntent(context, insightIntents).then(() => {
      Logger.info(TAG, 'shareIntent succeed');
      return 'share intent succeed';
    }, (err: BusinessError) => {
      Logger.error(TAG, `shareIntent error message: ${JSON.stringify(err)}`);
      return `shareIntent error message: ${JSON.stringify(err)}`;
    });
  }

  static async refreshDeleteIntentMessage(context: Context): Promise<string> {
    Logger.debug(TAG, 'refreshDeleteIntentMessage');
    try {
      return await FileReader.readJson(context, "deleteIntent.json");
    } catch (error) {
      Logger.error(TAG, `refresh delete intent message failed: ${JSON.stringify(error)}`);
      return 'invalid delete intent message';
    }
  }

  static async deleteIntent(context: Context, input: string): Promise<string> {
    Logger.debug(TAG, 'deleteIntent');
    let deleteIntent: DeleteIntent = JSON.parse(input);
    if (!deleteIntent){
      Logger.error(TAG, 'deleteIntent: json invalid.');
      return 'deleteIntent: json invalid.';
    }
    return await insightIntent.deleteIntent(context, deleteIntent.intentName, deleteIntent.identifiers).then(() => {
      Logger.info(TAG, 'deleteIntent succeed');
      return 'deleteIntent succeed';
    }, (err: BusinessError) => {
      Logger.error(TAG, `deleteIntent error message: ${JSON.stringify(err)}`);
      return `deleteIntent error message: ${JSON.stringify(err)}`;
    });
  }

  static async refreshDeleteEntityMessage(context: Context): Promise<string> {
    Logger.debug(TAG, 'refreshDeleteEntityMessage');
    try {
      return await FileReader.readJson(context, "deleteEntity.json");
    } catch (error) {
      Logger.error(TAG, `refresh delete entity message failed: ${JSON.stringify(error)}`);
      return 'invalid delete entity message';
    }
  }

  static async deleteEntity(context: Context, input: string): Promise<string> {
    Logger.debug(TAG, 'deleteEntity');
    let deleteEntity: DeleteEntity = JSON.parse(input);
    if (!deleteEntity){
      Logger.error(TAG, 'deleteEntity: json invalid.');
      return 'deleteEntity: json invalid.';
    }
    return await insightIntent.deleteEntity(context, deleteEntity.entityName, deleteEntity.entityIds).then(() => {
      Logger.info(TAG, 'deleteEntity succeed');
      return 'deleteEntity succeed';
    }, (err: BusinessError) => {
      Logger.error(TAG, `deleteEntity error message: ${JSON.stringify(err)}`);
      return `deleteEntity error message: ${JSON.stringify(err)}`;
    });
  }

  static async getSid(context: Context): Promise<string> {
    Logger.debug(TAG, 'getSid');
    try {
      return `SID: ${await insightIntent.getSid(context, false)}`;
    } catch (e) {
      return `getSid Error (${e.code}): ${e.message}`
    }
  }

  static async getSidRenew(context: Context): Promise<string> {
    Logger.debug(TAG, 'getSidRenew');
    try {
      return `SID(renew): ${await insightIntent.getSid(context, true)}`;
    } catch (e) {
      return `getSidRenew Error (${e.code}): ${e.message}`
    }
  }
}

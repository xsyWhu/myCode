import { Logger } from '../utils/Logger';

import { UIAbility } from '@kit.AbilityKit';
import type { AbilityConstant, Want } from '@kit.AbilityKit';
import type { window } from '@kit.ArkUI';
import type { BusinessError } from '@kit.BasicServicesKit';

const TAG: string = 'EntryAbility';

export default class EntryAbility extends UIAbility {
  private result: string = '';

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    Logger.info(TAG, 'onCreate');
    if (want.parameters?.['result']) {
      this.result = want.parameters?.['result'] as string;
    }
  }

  onDestroy(): void {
    Logger.info(TAG, 'onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    // Main window is created, set main page for this ability
    Logger.info(TAG, `onWindowStageCreate`)
    let para: Record<string, string> = {
      'result': this.result,
    };
    let localStorage: LocalStorage = new LocalStorage(para);
    windowStage.loadContent('pages/Index', localStorage)
      .then(() => {
        Logger.info(TAG, `onWindowStageCreate succeed`);
      }, (err: BusinessError) => {
        Logger.info(TAG, `onWindowStageCreate failes: ${JSON.stringify(err)}`);
      })
  }

  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    Logger.info(TAG, `onWindowStageDestroy`);
  }

  onForeground(): void {
    // Ability has brought to foreground
    Logger.info(TAG, `onForeground`);
  }

  onBackground(): void {
    // Ability has back to background
    Logger.info(TAG, `onBackground`);
  }
}

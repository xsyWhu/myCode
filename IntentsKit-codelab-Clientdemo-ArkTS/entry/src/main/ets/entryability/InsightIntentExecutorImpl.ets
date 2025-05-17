import { Logger } from '../utils/Logger';

import { insightIntent, InsightIntentExecutor } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import type { BusinessError } from '@kit.BasicServicesKit';

const TAG: string = 'InsightIntentExecutorImpl';

/**
 * 意图执行样例
 */
export default class InsightIntentExecutorImpl extends InsightIntentExecutor {
  private static readonly PLAY_MUSIC = 'PlayMusic';
  private static readonly VIEW_REPAYMENT = 'ViewRepayment';

  /**
   * override 执行前台UiAbility意图
   *
   * @param name 意图名称
   * @param param 意图参数
   * @param pageLoader 窗口
   * @returns 意图调用结果
   */
  onExecuteInUIAbilityForegroundMode(name: string, param: Record<string, Object>, pageLoader: window.WindowStage):
    Promise<insightIntent.ExecuteResult> {
    Logger.info(TAG, `onExecuteInUIAbilityForegroundMode name: ${name}, param: ${JSON.stringify(param)}`);
    // 根据意图名称分发处理逻辑
    switch (name) {
      case InsightIntentExecutorImpl.PLAY_MUSIC:
        return this.playMusic(param, pageLoader);
      case InsightIntentExecutorImpl.VIEW_REPAYMENT:
        return this.viewRepayment(param, pageLoader);
      default:
        break;
    }

    return Promise.resolve({
      code: -1,
      result: {
        message: 'unknown intent'
      }
    } as insightIntent.ExecuteResult)
  }

  /**
   * 实现调用播放音乐功能
   *
   * @param param 意图参数
   * @param pageLoader 窗口
   */
  private playMusic(param: Record<string, Object>, pageLoader: window.WindowStage): Promise<insightIntent.ExecuteResult> {
    return new Promise((resolve, reject) => {
      let para: Record<string, string> = {
        'result': `intent execute success, entityId: ${param.entityId}`
      };
      let localStorage: LocalStorage = new LocalStorage(para);
      // TODO 实现意图调用
      pageLoader.loadContent('pages/Index', localStorage)
        .then(() => {
          // TODO 调用成功的情况
          Logger.info(TAG, "Intent execute succeed");
          resolve({
            code: 0,
            result: {
              message: 'Intent execute succeed'
            }
          });
        })
        .catch((err: BusinessError) => {
          // TODO 调用失败的情况
          Logger.error(TAG, `Intent execute failed: ${JSON.stringify(err)}`);
          resolve({
            code: -1,
            result: {
              message: 'Intent execute failed'
            }
          })
        });
    })
  }

  /**
   * 实现调用查看还款功能
   *
   * @param param 意图参数
   * @param pageLoader 窗口
   */
  private viewRepayment(param: Record<string, Object>, pageLoader: window.WindowStage): Promise<insightIntent.ExecuteResult> {
    return new Promise((resolve, reject) => {
      let para: Record<string, string> = {
        'result': JSON.stringify(param)
      };
      let localStorage: LocalStorage = new LocalStorage(para);
      // TODO 实现意图调用
      pageLoader.loadContent('pages/Index', localStorage)
        .then(() => {
          // TODO 调用成功的情况
          Logger.info(TAG, "Intent execute succeed");
          resolve({
            code: 0,
            result: {
              message: 'Intent execute succeed'
            }
          });
        })
        .catch((err: BusinessError) => {
          // TODO 调用失败的情况
          Logger.error(TAG, `Intent execute failed: ${JSON.stringify(err)}`);
          resolve({
            code: -1,
            result: {
              message: 'Intent execute failed'
            }
          })
        });
    })
  }
}

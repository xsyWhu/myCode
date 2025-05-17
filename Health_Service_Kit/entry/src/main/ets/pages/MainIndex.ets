import { router } from '@kit.ArkUI';

@Entry
@Component
struct Index {
  @State message: string = 'Hello World';

  build() {
    Column() {
      Text($r('app.string.Kit')).fontWeight(FontWeight.Bold).fontSize(40);
      Button($r('app.string.ExerciseSequence')).width('50%').onClick(() => {
        router.pushUrl({
          url: 'pages/ExerciseSequenceIndex'
        })
      });
      Button($r('app.string.Auth')).width('50%').onClick(() => {
        router.pushUrl({
          url: 'pages/AuthIndex'
        })
      });
    }.alignItems(HorizontalAlign.Center).height('100%').width('100%')
    .justifyContent(FlexAlign.SpaceEvenly)
  }
}
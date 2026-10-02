package model

import (
	"errors"
	"ftns-asst/internal/util"
	"time"

	"github.com/google/uuid"
)

type (
	MuscleGroup string
	Equipment   string
	Exercise    struct {
		ID           uuid.UUID     `db:"id"`
		OwnerID      uuid.UUID     `db:"owner_id"`
		IsPublic     bool          `db:"is_public"`
		Name         string        `db:"name"`
		Description  string        `db:"description"`
		MuscleGroups []MuscleGroup `db:"muscle_group"`
		Equipments   []Equipment   `db:"equipment"`
		CreatedAt    time.Time     `db:"created_at"`
	}
	ExerciseListType string
	ExerciseFilters  struct {
		ListType     ExerciseListType
		UserID       *uuid.UUID    // for own and user list types
		MuscleGroups []MuscleGroup // filter by muscle groups
		Equipments   []Equipment   // filter by available equipments
	}

	UpdateExerciseInput struct {
		ID           uuid.UUID
		IsPublic     *bool
		Name         *string
		Description  *string
		MuscleGroups []MuscleGroup
		Equipments   []Equipment
	} // @Name UpdateExerciseRequestBody
)

const (
	ExerciseListTypeAll        ExerciseListType = "all"         // basic + own + public
	ExerciseListTypeOwn        ExerciseListType = "own"         // only own
	ExerciseListTypePublic     ExerciseListType = "public"      // all public
	ExerciseListTypePublicUser ExerciseListType = "public_user" // only public exercises of user
	ExerciseListTypeBasic      ExerciseListType = "basic"       // only basic
)

var (
	ErrUserIDRequired = errors.New("user id is required for this list type")
)

func (e *ExerciseFilters) ValidateAndDedup() error {
	if e.UserID == nil && (e.ListType == ExerciseListTypeOwn || e.ListType == ExerciseListTypePublicUser) {
		return ErrUserIDRequired
	}
	e.MuscleGroups = util.Dedup(e.MuscleGroups, func(m MuscleGroup) MuscleGroup { return m })
	e.Equipments = util.Dedup(e.Equipments, func(e Equipment) Equipment { return e })
	return nil
}

const (
	MuscleGroupChest      MuscleGroup = "chest"       // грудные
	MuscleGroupUpperBack  MuscleGroup = "upper_back"  // верх спины
	MuscleGroupLowerBack  MuscleGroup = "lower_back"  // широчайшие
	MuscleGroupLats       MuscleGroup = "lats"        // поясница
	MuscleGroupFrontDelts MuscleGroup = "front_delts" // передние дельты
	MuscleGroupSideDelts  MuscleGroup = "side_delts"  // средние дельты
	MuscleGroupRearDelts  MuscleGroup = "rear_delts"  // задние дельты
	MuscleGroupBiceps     MuscleGroup = "biceps"      // бицепс
	MuscleGroupTriceps    MuscleGroup = "triceps"     // трицепс
	MuscleGroupTraps      MuscleGroup = "traps"       // трапеции
	MuscleGroupForearms   MuscleGroup = "forearms"    // предплечья
	MuscleGroupAbs        MuscleGroup = "abs"         // пресс
	MuscleGroupOblique    MuscleGroup = "obliques"    // косые мышцы живота
	MuscleGroupHamstrings MuscleGroup = "hamstrings"  // бицепс бедра
	MuscleGroupGlutes     MuscleGroup = "glutes"      // ягодицы
	MuscleGroupQuads      MuscleGroup = "quads"       // квадрицепсы
	MuscleGroupAdductors  MuscleGroup = "adductors"   // приводящие мышцы бедра
	MuscleGroupAbductors  MuscleGroup = "abductors"   // отводящие мышцы бедра
	MuscleGroupCalves     MuscleGroup = "calves"      // икры
	MuscleGroupCardio     MuscleGroup = "cardio"      // кардио (сердечно-сосудистая система)
)

var MuscleGroupList = NewSet(
	MuscleGroupChest,
	MuscleGroupUpperBack,
	MuscleGroupLowerBack,
	MuscleGroupLats,
	MuscleGroupFrontDelts,
	MuscleGroupSideDelts,
	MuscleGroupRearDelts,
	MuscleGroupBiceps,
	MuscleGroupTriceps,
	MuscleGroupTraps,
	MuscleGroupForearms,
	MuscleGroupAbs,
	MuscleGroupOblique,
	MuscleGroupHamstrings,
	MuscleGroupGlutes,
	MuscleGroupQuads,
	MuscleGroupAdductors,
	MuscleGroupAbductors,
	MuscleGroupCalves,
	MuscleGroupCardio,
)

const (
	EquipmentBodyweight          Equipment = "bodyweight"            // Собственный вес
	EquipmentBarbell             Equipment = "barbell"               // Штанга
	EquipmentEZBar               Equipment = "ez_bar"                // EZ-гриф
	EquipmentDumbbell            Equipment = "dumbbell"              // Гантели
	EquipmentKettlebell          Equipment = "kettlebell"            // Гиря
	EquipmentBench               Equipment = "bench"                 // Скамья
	EquipmentInclineBench        Equipment = "incline_bench"         // Наклонная скамья
	EquipmentSquatRack           Equipment = "squat_rack"            // Силовая рама / стойки
	EquipmentSmithMachine        Equipment = "smith_machine"         // Машина Смита
	EquipmentCableMachine        Equipment = "cable_machine"         // Блочный тренажёр (кроссовер)
	EquipmentLatPulldownMachine  Equipment = "lat_pulldown_machine"  // Тренажёр верхней тяги
	EquipmentSeatedRowMachine    Equipment = "seated_row_machine"    // Тренажёр горизонтальной тяги
	EquipmentLegPressMachine     Equipment = "leg_press_machine"     // Тренажёр для жима ногами
	EquipmentLegExtensionMachine Equipment = "leg_extension_machine" // Тренажёр для разгибания ног
	EquipmentLegCurlMachine      Equipment = "leg_curl_machine"      // Тренажёр для сгибания ног
	EquipmentChestPressMachine   Equipment = "chest_press_machine"   // Тренажёр для жима от груди
	EquipmentPecDeckMachine      Equipment = "pec_deck_machine"      // Тренажёр «Бабочка»
	EquipmentHackSquatMachine    Equipment = "hack_squat_machine"    // Гакк-тренажёр
	EquipmentCalfRaiseMachine    Equipment = "calf_raise_machine"    // Тренажёр для икр
	EquipmentPullUpBar           Equipment = "pull_up_bar"           // Турник
	EquipmentDipBars             Equipment = "dip_bars"              // Брусья
	EquipmentRomanChair          Equipment = "roman_chair"           // Римский стул / гиперэкстензия
	EquipmentResistanceBand      Equipment = "resistance_band"       // Резиновая лента
	EquipmentMedicineBall        Equipment = "medicine_ball"         // Медбол
	EquipmentAbWheel             Equipment = "ab_wheel"              // Ролик для пресса
	EquipmentJumpRope            Equipment = "jump_rope"             // Скакалка
	EquipmentTreadmill           Equipment = "treadmill"             // Беговая дорожка
	EquipmentExerciseBike        Equipment = "exercise_bike"         // Велотренажёр
	EquipmentRowingMachine       Equipment = "rowing_machine"        // Гребной тренажёр
	EquipmentElliptical          Equipment = "elliptical"            // Эллипсоид
	EquipmentMat                 Equipment = "mat"                   // Коврик
)

var EquipmentList = NewSet(
	EquipmentBodyweight,
	EquipmentBarbell,
	EquipmentEZBar,
	EquipmentDumbbell,
	EquipmentKettlebell,
	EquipmentBench,
	EquipmentInclineBench,
	EquipmentSquatRack,
	EquipmentSmithMachine,
	EquipmentCableMachine,
	EquipmentLatPulldownMachine,
	EquipmentSeatedRowMachine,
	EquipmentLegPressMachine,
	EquipmentLegExtensionMachine,
	EquipmentLegCurlMachine,
	EquipmentChestPressMachine,
	EquipmentPecDeckMachine,
	EquipmentHackSquatMachine,
	EquipmentCalfRaiseMachine,
	EquipmentPullUpBar,
	EquipmentDipBars,
	EquipmentRomanChair,
	EquipmentResistanceBand,
	EquipmentMedicineBall,
	EquipmentAbWheel,
	EquipmentJumpRope,
	EquipmentTreadmill,
	EquipmentExerciseBike,
	EquipmentRowingMachine,
	EquipmentElliptical,
	EquipmentMat,
)
